package auris

/*
#include "auris/auris.h"
*/
import "C"

import (
	"runtime"
	"sync"
	"sync/atomic"
)

// Engine is one native pipeline. Control methods are safe from any goroutine.
type Engine struct {
	mu      sync.RWMutex
	c       *C.auris_engine_t
	running bool
	done    chan struct{}
	wg      sync.WaitGroup
	pushMu  sync.Mutex

	events  chan Event
	packets chan Packet

	droppedEvents  atomic.Uint64
	droppedPackets atomic.Uint64
}

// New creates an engine for cfg. It is not capturing until Start.
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
	runtime.SetFinalizer(e, func(e *Engine) { _ = e.Close() })
	return e, nil
}

// Close stops if needed, destroys the native engine and closes both
// channels. Idempotent.
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
	C.auris_destroy(e.c)
	e.c = nil
	runtime.SetFinalizer(e, nil)
	close(e.events)
	close(e.packets)
	return nil
}

// Events delivers detections in order. Closed by Close.
func (e *Engine) Events() <-chan Event { return e.events }

// Packets delivers encoded audio, each a Go-owned copy. Closed by Close.
func (e *Engine) Packets() <-chan Packet { return e.packets }

// Dropped counts events and packets discarded because a channel was full.
func (e *Engine) Dropped() (events, packets uint64) {
	return e.droppedEvents.Load(), e.droppedPackets.Load()
}
