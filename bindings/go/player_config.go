// PlayerConfig: everything a Player needs, and its conversion to the C struct.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import (
	"math"
	"time"
)

// PlayerConfig is everything a Player needs. NewPlayer starts from
// DefaultPlayerConfig and applies its options.
type PlayerConfig struct {
	// Backend is BackendAuto, a device backend, or BackendHostPCM to pull
	// the output with Player.Read.
	Backend Backend
	// SampleRateHz is 8000, 12000, 16000, 24000 or 48000.
	SampleRateHz uint32
	// Channels is 1 or 2.
	Channels uint16
	// TargetDelay is the audio buffered before playout, 0 to 2 s.
	TargetDelay time.Duration
	// MaxDelay is the most buffered before the oldest drops, target to 10 s.
	MaxDelay time.Duration
	// ConcealMax is the longest hole concealed, 0 to 1 s.
	ConcealMax time.Duration
	// QueuePackets is the number of packet slots, 4 to 4096.
	QueuePackets uint32
}

// DefaultPlayerConfig mirrors the native defaults: the platform's output at
// 16 kHz mono, 60 ms target, 400 ms max, 100 ms concealment, 128 slots.
func DefaultPlayerConfig() PlayerConfig {
	var c C.mockingbird_player_config_t
	C.mockingbird_player_config_defaults(&c)
	return PlayerConfig{
		Backend:      Backend(c.backend),
		SampleRateHz: uint32(c.sample_rate_hz),
		Channels:     uint16(c.channels),
		TargetDelay:  time.Duration(c.target_delay_ms) * time.Millisecond,
		MaxDelay:     time.Duration(c.max_delay_ms) * time.Millisecond,
		ConcealMax:   time.Duration(c.conceal_max_ms) * time.Millisecond,
		QueuePackets: uint32(c.queue_packets),
	}
}

func durationToMs(d time.Duration) C.uint32_t {
	ms := d.Milliseconds()
	if ms < 0 || ms > math.MaxUint32 {
		return C.uint32_t(math.MaxUint32)
	}
	return C.uint32_t(ms)
}

func (cfg PlayerConfig) toC() C.mockingbird_player_config_t {
	var c C.mockingbird_player_config_t
	C.mockingbird_player_config_defaults(&c)
	c.backend = C.mockingbird_backend_t(cfg.Backend)
	c.sample_rate_hz = C.uint32_t(cfg.SampleRateHz)
	c.channels = C.uint16_t(cfg.Channels)
	c.target_delay_ms = durationToMs(cfg.TargetDelay)
	c.max_delay_ms = durationToMs(cfg.MaxDelay)
	c.conceal_max_ms = durationToMs(cfg.ConcealMax)
	c.queue_packets = C.uint32_t(cfg.QueuePackets)
	return c
}
