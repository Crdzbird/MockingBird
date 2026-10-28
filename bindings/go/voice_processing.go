package mockingbird

// VoiceProcessing selects the platform's own processing for live capture.
// Each platform applies what it has; Stats.Capture.VoiceProcessing reports
// what is in effect.
type VoiceProcessing uint32

const (
	// VoiceEchoCancel removes the device's own playback from the input.
	VoiceEchoCancel VoiceProcessing = 1 << iota
	// VoiceGainControl levels the input automatically.
	VoiceGainControl
	// VoiceNoiseSuppress runs the platform's noise suppressor.
	VoiceNoiseSuppress
)

// VoiceAll turns on every stage, as voice and video calls use.
const VoiceAll = VoiceEchoCancel | VoiceGainControl | VoiceNoiseSuppress
