package auris

// SoundClassConfig drives YAMNet classification over all 521 AudioSet classes.
type SoundClassConfig struct {
	Enabled bool
	// ModelPath locates yamnet_3s.onnx.
	ModelPath string
	// Threshold is the top-1 probability needed to emit an event.
	Threshold float32
	// IntervalMs is the inference cadence; halving it roughly halves the cost.
	IntervalMs uint32
}

// DefaultSoundClassConfig is off, threshold 0.4, one inference per second.
func DefaultSoundClassConfig() SoundClassConfig {
	return SoundClassConfig{Threshold: 0.4, IntervalMs: 1000}
}

func (c SoundClassConfig) sensitive(s Sensitivity) SoundClassConfig {
	c.Enabled = true
	c.Threshold = float32(s.Lerp(0.70, 0.12))
	return c
}
