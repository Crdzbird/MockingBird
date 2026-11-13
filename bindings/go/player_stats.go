// PlayerStats: a snapshot of a Player's jitter buffer and output counters.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// PlayerStats is a snapshot of a player's counters. It never carries audio.
type PlayerStats struct {
	// PacketsReceived counts packets accepted by Push.
	PacketsReceived uint64
	PacketsPlayed   uint64
	// PacketsConcealed counts missing packets replaced by concealment.
	PacketsConcealed uint64
	// PacketsDropped counts late, duplicate, over max delay, no free slot
	// and undecodable packets.
	PacketsDropped uint64
	// PacketsLate is the late and duplicate share of PacketsDropped.
	PacketsLate uint64
	Underruns   uint64
	// FramesRendered counts frames given to the output, silence too.
	FramesRendered  uint64
	BufferedPackets uint32
	BufferedMs      uint32
	State           PlayerState
	// OutputRestarts counts route changes the output followed.
	OutputRestarts uint32
}

func playerStatsFromC(s C.mockingbird_player_stats_t) PlayerStats {
	return PlayerStats{
		PacketsReceived:  uint64(s.packets_received),
		PacketsPlayed:    uint64(s.packets_played),
		PacketsConcealed: uint64(s.packets_concealed),
		PacketsDropped:   uint64(s.packets_dropped),
		PacketsLate:      uint64(s.packets_late),
		Underruns:        uint64(s.underruns),
		FramesRendered:   uint64(s.frames_rendered),
		BufferedPackets:  uint32(s.buffered_packets),
		BufferedMs:       uint32(s.buffered_ms),
		State:            PlayerState(s.state),
		OutputRestarts:   uint32(s.output_restarts),
	}
}
