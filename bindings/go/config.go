package auris

// Config is everything an Engine needs, grouped by concern. DefaultConfig
// gives fast VAD and the anomaly gate on the platform's microphone.
type Config struct {
	Backend      Backend
	SampleRateHz uint32
	FrameMs      uint32
	// Denoise runs RNNoise ahead of every detector and the encoder; the
	// sample rate must then be 8, 16 or 48 kHz.
	Denoise     bool
	VAD         VadConfig
	Anomaly     AnomalyConfig
	SoundClass  SoundClassConfig
	Segments    SegmentConfig
	Encoder     EncoderConfig
	Queues      QueueConfig
	Performance PerformanceConfig
}

// DefaultConfig mirrors the native defaults: 16 kHz mono, 20 ms frames.
func DefaultConfig() Config {
	return Config{
		Backend:      BackendAuto,
		SampleRateHz: 16000,
		FrameMs:      20,
		VAD:          DefaultVadConfig(),
		Anomaly:      DefaultAnomalyConfig(),
		SoundClass:   DefaultSoundClassConfig(),
		Segments:     DefaultSegmentConfig(),
		Encoder:      DefaultEncoderConfig(),
		Queues:       DefaultQueueConfig(),
		Performance:  DefaultPerformanceConfig(),
	}
}
