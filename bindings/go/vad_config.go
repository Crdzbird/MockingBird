package auris

// VadConfig drives voice-activity detection: the language-agnostic
// EventSpeechBegin and EventSpeechEnd events.
type VadConfig struct {
	Enabled bool
	Tier    VadTier
	// Aggressiveness is libfvad's 0..3; higher rejects borderline frames.
	Aggressiveness int
	// NeuralThreshold is Silero's P(speech) threshold; lower is more sensitive.
	NeuralThreshold float32
	// HangoverMs is the trailing silence before a speech segment closes.
	HangoverMs uint32
	// ModelPath locates silero_vad.onnx for VadNeural.
	ModelPath string
}

// DefaultVadConfig is fast VAD, aggressiveness 2, 300 ms hangover.
func DefaultVadConfig() VadConfig {
	return VadConfig{Enabled: true, Tier: VadFast, Aggressiveness: 2, NeuralThreshold: 0.5, HangoverMs: 300}
}

func (v VadConfig) sensitive(s Sensitivity) VadConfig {
	v.Enabled = true
	v.Tier = VadNeural
	v.Aggressiveness = int(s.Lerp(3, 0) + 0.5)
	v.NeuralThreshold = float32(s.Lerp(0.80, 0.20))
	return v
}
