package mockingbird

// PacketFlag bits describe a packet's place in its segment.
type PacketFlag uint32

const (
	// PacketFlagPreroll marks audio captured before the trigger.
	PacketFlagPreroll PacketFlag = 1 << iota
	// PacketFlagPostroll marks trailing audio after the detector closed.
	PacketFlagPostroll
	// PacketFlagKeyframe marks the first encoded frame of a segment.
	PacketFlagKeyframe
	// PacketFlagLast marks the terminal packet of a segment.
	PacketFlagLast
)
