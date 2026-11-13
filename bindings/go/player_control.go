// Player controls: volume, mute and stats, safe from any goroutine.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// SetVolume sets the linear gain in [0, 1], applied with a short ramp.
func (p *Player) SetVolume(volume float32) error {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return ErrClosed
	}
	if st := Status(C.mockingbird_player_set_volume(p.c, C.float(volume))); st != StatusOK {
		return st.err("mockingbird_player_set_volume")
	}
	return nil
}

// SetMuted mutes or unmutes the output; muting keeps the volume.
func (p *Player) SetMuted(muted bool) error {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return ErrClosed
	}
	if st := Status(C.mockingbird_player_set_muted(p.c, boolToC(muted))); st != StatusOK {
		return st.err("mockingbird_player_set_muted")
	}
	return nil
}

// Stats snapshots the player's counters and state.
func (p *Player) Stats() (PlayerStats, error) {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return PlayerStats{}, ErrClosed
	}
	var s C.mockingbird_player_stats_t
	if st := Status(C.mockingbird_player_get_stats(p.c, &s)); st != StatusOK {
		return PlayerStats{}, st.err("mockingbird_player_get_stats")
	}
	return playerStatsFromC(s), nil
}
