package auris

// PerformanceConfig trades latency for CPU and battery.
type PerformanceConfig struct {
	// SilenceGateDbfs: frames below this level skip RNNoise, and YAMNet skips
	// an inference whose whole window was quiet. -120 or lower disables.
	SilenceGateDbfs float32
	// AdaptiveDegradation sheds YAMNet work under load and restores it when
	// calm. Off by default: it reacts to wall-clock time, which offline runs
	// must not depend on.
	AdaptiveDegradation bool
	// DegradeBusyPpm is the worker busy share that triggers degradation.
	DegradeBusyPpm uint32
	// WorkerBatchFrames wakes the worker only once this many frames are
	// queued: the same CPU work in fewer wakeups, for a little latency.
	WorkerBatchFrames uint32
}

// DefaultPerformanceConfig gates at -60 dBFS with degradation off.
func DefaultPerformanceConfig() PerformanceConfig {
	return PerformanceConfig{SilenceGateDbfs: -60, DegradeBusyPpm: 700000}
}

// MobilePerformance enables degradation and batches four frames per wakeup.
func MobilePerformance() PerformanceConfig {
	p := DefaultPerformanceConfig()
	p.AdaptiveDegradation = true
	p.WorkerBatchFrames = 4
	return p
}
