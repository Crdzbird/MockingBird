package auris

/*
#include "auris/auris.h"
*/
import "C"

// UpdateConfig retunes a running engine in place: detector toggles and
// thresholds, the silence gate, degradation and wake batching. Detector
// state survives. Structural fields must match the creating config, and a
// detector that was off at creation cannot be switched on.
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

// Stats snapshots the engine's counters.
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
