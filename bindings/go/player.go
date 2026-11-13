// Player handle: creating and releasing a native jitter-buffered output.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import (
	"runtime"
	"sync"
)

// Player plays Opus or PCM packets on the device's output through a jitter
// buffer ordered by pts. Every method is safe from any goroutine: lifecycle
// calls are serialised, concurrent pushes are serialised, and so are reads.
type Player struct {
	mu       sync.RWMutex
	c        *C.mockingbird_player_t
	feedMu   sync.Mutex
	readMu   sync.Mutex
	rate     int
	channels int
}

// NewPlayer creates a player from DefaultPlayerConfig adjusted by opts. It
// plays nothing until Start; packets pushed before then wait.
func NewPlayer(opts ...PlayerOption) (*Player, error) {
	cfg := DefaultPlayerConfig()
	for _, o := range opts {
		o(&cfg)
	}
	ccfg := cfg.toC()
	var c *C.mockingbird_player_t
	if st := Status(C.mockingbird_player_create(&ccfg, &c)); st != StatusOK {
		return nil, st.err("mockingbird_player_create")
	}
	p := &Player{c: c, rate: int(cfg.SampleRateHz), channels: int(cfg.Channels)}
	runtime.SetFinalizer(p, func(p *Player) { _ = p.Close() })
	return p, nil
}

// Close stops if needed and destroys the native player. Idempotent.
func (p *Player) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.c == nil {
		return nil
	}
	C.mockingbird_player_destroy(p.c)
	p.c = nil
	runtime.SetFinalizer(p, nil)
	return nil
}
