package auris

/*
#include "auris/auris.h"
*/
import "C"

import "unsafe"

// PushPCM feeds int16 mono PCM into a BackendHostPCM engine. Concurrent
// pushes are serialised to honour the single-producer ring.
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

// ReadPCM drains the ungated PCM tap into out and returns the sample count.
// Needs QueueConfig.PCMTap; call it from one goroutine.
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
