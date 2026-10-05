package mockingbird

// Option adjusts a Config built by Sensitive.
type Option func(*Config)

// WithBackend sets the audio source.
func WithBackend(b Backend) Option { return func(c *Config) { c.Backend = b } }

// WithSampleRate sets the working sample rate.
func WithSampleRate(hz uint32) Option { return func(c *Config) { c.SampleRateHz = hz } }

// WithDenoise toggles RNNoise ahead of every detector.
func WithDenoise(on bool) Option { return func(c *Config) { c.Denoise = on } }

// WithVadTier picks the voice detector; Sensitive defaults to VadNeural.
func WithVadTier(t VadTier) Option { return func(c *Config) { c.VAD.Tier = t } }

// WithNeuralVADModel sets the Silero ONNX model path.
func WithNeuralVADModel(path string) Option {
	return func(c *Config) { c.VAD.ModelPath = path }
}

// WithSoundClassModel sets the YAMNet ONNX model path.
func WithSoundClassModel(path string) Option {
	return func(c *Config) { c.SoundClass.ModelPath = path }
}

// WithDetectors switches the three detectors on or off together.
func WithDetectors(vad, anomaly, soundClass bool) Option {
	return func(c *Config) {
		c.VAD.Enabled = vad
		c.Anomaly.Enabled = anomaly
		c.SoundClass.Enabled = soundClass
	}
}

// WithPCMTap enables the ungated PCM tap; 0 frames means one second.
func WithPCMTap(frames uint32) Option {
	return func(c *Config) {
		c.Queues.PCMTap = true
		c.Queues.PCMTapFrames = frames
	}
}

// WithEncoder replaces the encoder settings.
func WithEncoder(e EncoderConfig) Option { return func(c *Config) { c.Encoder = e } }

// WithPerformance replaces the performance settings.
func WithPerformance(p PerformanceConfig) Option {
	return func(c *Config) { c.Performance = p }
}
