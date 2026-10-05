package mockingbird

// Codec selects how packets are encoded.
type Codec int

const (
	// CodecPCMS16LE is raw 16-bit little-endian PCM.
	CodecPCMS16LE Codec = iota
	// CodecOpus is Opus, the usual choice for live voice.
	CodecOpus
	// CodecFLAC is declared by the ABI and not implemented yet.
	CodecFLAC
)

// String names the codec.
func (c Codec) String() string {
	switch c {
	case CodecPCMS16LE:
		return "pcm"
	case CodecOpus:
		return "opus"
	case CodecFLAC:
		return "flac"
	}
	return "unknown"
}
