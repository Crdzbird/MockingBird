package auris

// EncoderConfig selects how segment audio is encoded for the host.
type EncoderConfig struct {
	Codec          Codec
	OpusBitrateBps uint32
	OpusVBR        bool
	// OpusComplexity is 0..10; higher is better quality for more CPU.
	OpusComplexity int
}

// DefaultEncoderConfig is PCM passthrough with Opus settings ready to use.
func DefaultEncoderConfig() EncoderConfig {
	return EncoderConfig{Codec: CodecPCMS16LE, OpusBitrateBps: 32000, OpusVBR: true, OpusComplexity: 5}
}

// OpusEncoder is Opus at 32 kbps VBR, the usual choice for live voice.
func OpusEncoder() EncoderConfig {
	e := DefaultEncoderConfig()
	e.Codec = CodecOpus
	return e
}
