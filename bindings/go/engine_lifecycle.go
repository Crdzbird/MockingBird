package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// Start begins capture and detection and launches the drain goroutine.
func (e *Engine) Start() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.c == nil {
		return ErrClosed
	}
	if e.running {
		return StatusErrAlreadyRunning.err("mockingbird_start")
	}
	if st := Status(C.mockingbird_start(e.c)); st != StatusOK {
		return st.err("mockingbird_start")
	}
	e.running = true
	e.done = make(chan struct{})
	e.wg.Add(1)
	go e.pump(e.c, e.done)
	return nil
}

// Stop halts capture. The native engine flushes its tail first, then the
// drain goroutine delivers it, so no packet produced before Stop is lost.
func (e *Engine) Stop() error {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.c == nil {
		return ErrClosed
	}
	if !e.running {
		return StatusErrNotRunning.err("mockingbird_stop")
	}
	st := Status(C.mockingbird_stop(e.c))
	close(e.done)
	e.running = false
	e.wg.Wait()
	if st != StatusOK {
		return st.err("mockingbird_stop")
	}
	return nil
}
