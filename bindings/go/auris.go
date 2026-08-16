// Package auris is an idiomatic Go binding for the Auris native audio capture
// and detection engine. It wraps the library's pure C ABI via cgo: capture →
// optional RNNoise → VAD + anomaly + YAMNet classification → encode, with
// language-agnostic voice detection.
//
// # Linking
//
// The binding links libauris (built from this repository with CMake). By
// default it expects the shared library and headers at the repo-relative paths
// baked into the cgo directives below; override with CGO_CFLAGS / CGO_LDFLAGS
// if you install libauris elsewhere. See the package README for details.
//
// # Lifecycle & threading
//
// Create an [Engine] with [New], drive it with [Engine.Start]/[Engine.Stop],
// consume detections from [Engine.Events] and encoded audio from
// [Engine.Packets], and ALWAYS release it with [Engine.Close]. The control
// methods (Start/Stop/Close/PushPCM/Stats) are safe to call from any
// goroutine and are internally synchronized; the Events and Packets channels
// may be read from any goroutine. An internal goroutine drains the native
// queues, so a slow consumer never stalls the engine — surplus is dropped
// (observable via [Engine.Dropped]) the same way the native side drops-oldest.
package auris

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lauris -Wl,-rpath,${SRCDIR}/../../build

#include <stdlib.h>
#include "auris/auris.h"
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

// ErrClosed is returned by methods called on an engine that has been closed.
var ErrClosed = errors.New("auris: engine is closed")

// Version returns the "X.Y.Z" version of the linked libauris.
func Version() string { return C.GoString(C.auris_version_string()) }

// Engine is a running (or runnable) Auris pipeline.
type Engine struct {
	// mu guards the validity of c and the lifecycle fields. Read-locked while a
	// caller uses c for a native call (PushPCM/Stats) so Close cannot destroy
	// it mid-call; write-locked for Start/Stop/Close.
	mu      sync.RWMutex
	c       *C.auris_engine_t
	running bool
	done    chan struct{}
	wg      sync.WaitGroup

	// pushMu serializes PushPCM: the native PCM ring is single-producer, so
	// concurrent pushes must be serialized to avoid corrupting it.
	pushMu sync.Mutex

	events  chan Event
	packets chan Packet

	droppedEvents  atomic.Uint64
	droppedPackets atomic.Uint64
}

// New constructs an engine from cfg. The engine is not yet capturing; call
// Start. On any validation failure it returns a non-nil error and leaks
// nothing.
func New(cfg Config) (*Engine, error) {
	ccfg, free := cfg.toC()
	defer free()

	var c *C.auris_engine_t
	if st := Status(C.auris_create(&ccfg, &c)); st != StatusOK {
		return nil, st.err("auris_create")
	}
	e := &Engine{
		c:       c,
		events:  make(chan Event, 256),
		packets: make(chan Packet, 256),
	}
	// Backstop for a forgotten Close (only reachable when the engine was never
	// started or was already stopped, since a running pump keeps e alive).
	runtime.SetFinalizer(e, func(e *Engine) { _ = e.Close() })
	return e, nil
}

// Start begins capture + detection and spins up the drain goroutine.
func (e *Engine) Start() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.c == nil {
		return ErrClosed
	}
	if e.running {
		return StatusErrAlreadyRunning.err("auris_start")
	}
	if st := Status(C.auris_start(e.c)); st != StatusOK {
		return st.err("auris_start")
	}
	e.running = true
	e.done = make(chan struct{})
	e.wg.Add(1)
	go e.pump(e.c, e.done)
	return nil
}

// Stop halts capture. Queued packets up to the channel buffer remain readable;
// the rest are released on Close. Returns an error if not running.
func (e *Engine) Stop() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.c == nil {
		return ErrClosed
	}
	if !e.running {
		return StatusErrNotRunning.err("auris_stop")
	}
	// Stop the native engine FIRST: its worker drains the PCM ring and flushes
	// every remaining packet into the queue before this returns. THEN wind down
	// the pump, whose final drain delivers those flushed packets to the channels
	// — otherwise the tail of a stream would be silently lost (the ABI promises
	// post-stop packets stay drainable).
	st := Status(C.auris_stop(e.c))
	close(e.done)
	e.running = false
	e.wg.Wait() // pump's final drainOnce delivers the flush, then exits
	if st != StatusOK {
		return st.err("auris_stop")
	}
	return nil
}

// Close stops (if needed) and destroys the engine, releasing every native
// resource and closing the Events and Packets channels. Idempotent; the engine
// must not be used afterward.
func (e *Engine) Close() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.c == nil {
		return nil
	}
	if e.running {
		close(e.done)
		e.running = false
		e.wg.Wait()
	}
	C.auris_destroy(e.c) // auto-stops if still running
	e.c = nil
	runtime.SetFinalizer(e, nil)
	close(e.events)
	close(e.packets)
	return nil
}

// PushPCM feeds int16 mono PCM into a HOST_PCM engine. captureTsUs is the
// monotonic timestamp of the first sample. Safe to call concurrently — pushes
// are serialized to satisfy the single-producer PCM ring.
func (e *Engine) PushPCM(samples []int16, captureTsUs uint64) error {
	if len(samples) == 0 {
		return nil
	}
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	e.pushMu.Lock()
	defer e.pushMu.Unlock()
	st := Status(C.auris_push_pcm(
		e.c,
		(*C.int16_t)(unsafe.Pointer(&samples[0])),
		C.uint32_t(len(samples)),
		C.uint64_t(captureTsUs),
	))
	if st != StatusOK {
		return st.err("auris_push_pcm")
	}
	return nil
}

// ReadPCM drains the continuous PCM tap into out and returns how many samples
// were written (0 when nothing is buffered). Requires Config.EnablePCMTap.
//
// Unlike Packets, the tap is NOT gated by segments: it carries every frame the
// worker processed, so you can render a waveform or archive the whole stream
// while still receiving detections and encoded packets. Returns
// StatusErrUnsupported wrapped in an error if the tap is disabled. Call from
// one goroutine (single-consumer, like the packet drain).
func (e *Engine) ReadPCM(out []int16) (int, error) {
	if len(out) == 0 {
		return 0, nil
	}
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return 0, ErrClosed
	}
	var written C.uint32_t
	st := Status(C.auris_read_pcm(
		e.c,
		(*C.int16_t)(unsafe.Pointer(&out[0])),
		C.uint32_t(len(out)),
		&written,
	))
	if st != StatusOK {
		return 0, st.err("auris_read_pcm")
	}
	return int(written), nil
}

// UpdateConfig retunes a live engine in place — no destroy/recreate, no model
// reload, and detector state (VAD hysteresis, the anomaly rolling background,
// the classifier EMA) is preserved.
//
// Only the tunable subset is applied: EnableVAD, EnableAnomaly, EnableSoundClass,
// VadAggressiveness, VadNeuralThreshold, VadHangoverMs, AnomalyRMSZ,
// AnomalyFluxZ, and SoundClassThreshold. Every other field is structural and
// must match what the engine was created with, so the usual pattern is to keep
// the original Config, change a tunable field, and pass it back:
//
//	cfg.AnomalyRMSZ = 2.0
//	err := engine.UpdateConfig(cfg)
//
// Returns an error if the config is invalid, a structural field differs, or it
// tries to switch on a detector that was off at create time (it was never
// constructed). Safe to call from any goroutine.
func (e *Engine) UpdateConfig(cfg Config) error {
	ccfg, free := cfg.toC()
	defer free()

	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return ErrClosed
	}
	if st := Status(C.auris_update_config(e.c, &ccfg)); st != StatusOK {
		return st.err("auris_update_config")
	}
	return nil
}

// Stats snapshots the engine's observability counters.
func (e *Engine) Stats() (Stats, error) {
	e.mu.RLock()
	defer e.mu.RUnlock()
	if e.c == nil {
		return Stats{}, ErrClosed
	}
	var s C.auris_stats_t
	if st := Status(C.auris_get_stats(e.c, &s)); st != StatusOK {
		return Stats{}, st.err("auris_get_stats")
	}
	return statsFromC(s), nil
}

// Events returns the channel of detection events. Closed by Close.
func (e *Engine) Events() <-chan Event { return e.events }

// Packets returns the channel of encoded audio packets. Each Packet owns a copy
// of its bytes (the native borrow is released immediately). Closed by Close.
func (e *Engine) Packets() <-chan Packet { return e.packets }

// Dropped reports how many events and packets the drain goroutine had to drop
// because the corresponding channel was full (a slow or absent consumer).
func (e *Engine) Dropped() (events, packets uint64) {
	return e.droppedEvents.Load(), e.droppedPackets.Load()
}

// pollInterval bounds the latency between native production and channel
// delivery. A fixed-interval poll (rather than a busy loop) keeps CPU flat even
// under a sustained backlog.
const pollInterval = 2 * time.Millisecond

// pump runs on its own goroutine. It owns the c pointer for its lifetime: Start
// launched it after auris_start succeeded, and Stop/Close join it (wg) BEFORE
// auris_stop/auris_destroy, so c is always valid here without locking. All
// channel sends are non-blocking (drop-if-full), so the pump never blocks and
// reacts to done promptly — Close can never deadlock on wg.Wait.
func (e *Engine) pump(c *C.auris_engine_t, done chan struct{}) {
	defer e.wg.Done()
	t := time.NewTicker(pollInterval)
	defer t.Stop()
	for {
		select {
		case <-done:
			e.drainOnce(c) // final sweep of whatever was already produced
			return
		case <-t.C:
			e.drainOnce(c)
		}
	}
}

func (e *Engine) drainOnce(c *C.auris_engine_t) bool {
	work := false
	for {
		var p C.auris_packet_t
		if Status(C.auris_drain(c, &p)) != StatusOK {
			break
		}
		pkt := packetFromC(p) // copies the bytes
		C.auris_release(c, &p)
		work = true
		select {
		case e.packets <- pkt:
		default:
			e.droppedPackets.Add(1)
		}
	}
	for {
		var ev C.auris_event_t
		if Status(C.auris_poll_event(c, &ev)) != StatusOK {
			break
		}
		work = true
		select {
		case e.events <- eventFromC(ev):
		default:
			e.droppedEvents.Add(1)
		}
	}
	return work
}

// Status mirrors auris_status_t.
type Status int

const (
	StatusOK                Status = 0
	StatusErrInvalidArg     Status = -1
	StatusErrInvalidState   Status = -2
	StatusErrNoMemory       Status = -3
	StatusErrBackend        Status = -4
	StatusErrNotRunning     Status = -5
	StatusErrAlreadyRunning Status = -6
	StatusErrQueueEmpty     Status = -7
	StatusErrPermission     Status = -8
	StatusErrUnsupported    Status = -9
	StatusErrInternal       Status = -10
)

// String returns the library's human-readable status name.
func (s Status) String() string { return C.GoString(C.auris_status_str(C.int(s))) }

// StatusError wraps a non-OK native status code. Use errors.As to inspect the
// Status and handle specific failures (e.g. StatusErrPermission):
//
//	var se *auris.StatusError
//	if errors.As(err, &se) && se.Status == auris.StatusErrPermission { ... }
type StatusError struct {
	Op     string // the native call that failed, e.g. "auris_start"
	Status Status
}

func (e *StatusError) Error() string {
	return fmt.Sprintf("auris: %s failed: %s (%d)", e.Op, e.Status, int(e.Status))
}

func (s Status) err(op string) error { return &StatusError{Op: op, Status: s} }
