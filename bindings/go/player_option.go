// PlayerOption: functional options that adjust the PlayerConfig of NewPlayer.

package mockingbird

import "time"

// PlayerOption adjusts the PlayerConfig built by NewPlayer.
type PlayerOption func(*PlayerConfig)

// WithPlayerConfig replaces the whole config; later options adjust it.
func WithPlayerConfig(cfg PlayerConfig) PlayerOption {
	return func(c *PlayerConfig) { *c = cfg }
}

// WithPlayerBackend sets the output; BackendHostPCM opens no device.
func WithPlayerBackend(b Backend) PlayerOption {
	return func(c *PlayerConfig) { c.Backend = b }
}

// WithPlayerRate sets the output sample rate.
func WithPlayerRate(hz uint32) PlayerOption {
	return func(c *PlayerConfig) { c.SampleRateHz = hz }
}

// WithPlayerChannels sets the output channel count, 1 or 2.
func WithPlayerChannels(n uint16) PlayerOption {
	return func(c *PlayerConfig) { c.Channels = n }
}

// WithTargetDelay sets how much audio is buffered before playout.
func WithTargetDelay(d time.Duration) PlayerOption {
	return func(c *PlayerConfig) { c.TargetDelay = d }
}

// WithMaxDelay sets the most audio buffered before the oldest packets drop.
func WithMaxDelay(d time.Duration) PlayerOption {
	return func(c *PlayerConfig) { c.MaxDelay = d }
}

// WithConcealMax sets the longest hole that is concealed rather than silent.
func WithConcealMax(d time.Duration) PlayerOption {
	return func(c *PlayerConfig) { c.ConcealMax = d }
}

// WithQueuePackets sets the number of packet slots in the jitter buffer.
func WithQueuePackets(n uint32) PlayerOption {
	return func(c *PlayerConfig) { c.QueuePackets = n }
}
