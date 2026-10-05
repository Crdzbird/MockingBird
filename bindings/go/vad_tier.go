package mockingbird

// VadTier selects the voice-activity detector. Both are language-agnostic.
type VadTier int

const (
	// VadFast is libfvad: zero-dependency and cheap.
	VadFast VadTier = iota
	// VadNeural is Silero v5 via ONNX Runtime.
	VadNeural
)

// String names the tier.
func (t VadTier) String() string {
	if t == VadNeural {
		return "neural"
	}
	return "fast"
}
