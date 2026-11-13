// Player lifecycle: starting, pausing, resuming and stopping the output.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// Start opens the output. A running player returns StatusErrAlreadyRunning.
func (p *Player) Start() error {
	return p.lifecycle("mockingbird_player_start", func() C.mockingbird_status_t {
		return C.mockingbird_player_start(p.c)
	})
}

// Pause stops the output and keeps what is buffered.
func (p *Player) Pause() error {
	return p.lifecycle("mockingbird_player_pause", func() C.mockingbird_status_t {
		return C.mockingbird_player_pause(p.c)
	})
}

// Resume starts the output again after Pause or a lost route.
func (p *Player) Resume() error {
	return p.lifecycle("mockingbird_player_resume", func() C.mockingbird_status_t {
		return C.mockingbird_player_resume(p.c)
	})
}

// Stop closes the output and drops everything buffered.
func (p *Player) Stop() error {
	return p.lifecycle("mockingbird_player_stop", func() C.mockingbird_status_t {
		return C.mockingbird_player_stop(p.c)
	})
}

func (p *Player) lifecycle(op string, call func() C.mockingbird_status_t) error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.c == nil {
		return ErrClosed
	}
	if st := Status(call()); st != StatusOK {
		return st.err(op)
	}
	return nil
}
