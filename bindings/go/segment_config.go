package mockingbird

// SegmentConfig shapes how detections are cut into segments of audio.
type SegmentConfig struct {
	// PreRollMs is audio kept from before the trigger.
	PreRollMs uint32
	// PostRollMs is audio kept after the detector closes.
	PostRollMs uint32
	// MaxMs caps one segment; 0 is unbounded.
	MaxMs uint32
}

// DefaultSegmentConfig keeps 300 ms either side and caps at a minute.
func DefaultSegmentConfig() SegmentConfig {
	return SegmentConfig{PreRollMs: 300, PostRollMs: 300, MaxMs: 60000}
}
