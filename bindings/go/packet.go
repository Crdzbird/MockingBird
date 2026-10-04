package auris

/*
#include "auris/auris.h"
*/
import "C"

import "unsafe"

// Packet is one encoded audio packet. Data is a Go-owned copy; the native
// borrow was released before the packet was delivered.
type Packet struct {
	SegmentID   uint64
	PtsUs       uint64
	CaptureTsUs uint64
	Flags       PacketFlag
	Codec       Codec
	Data        []byte
}

// IsPreroll reports audio captured before the trigger.
func (p Packet) IsPreroll() bool { return p.Flags&PacketFlagPreroll != 0 }

// IsPostroll reports trailing audio after the detector closed.
func (p Packet) IsPostroll() bool { return p.Flags&PacketFlagPostroll != 0 }

// IsKeyframe reports the first encoded frame of a segment.
func (p Packet) IsKeyframe() bool { return p.Flags&PacketFlagKeyframe != 0 }

// IsLast reports the terminal packet of a segment.
func (p Packet) IsLast() bool { return p.Flags&PacketFlagLast != 0 }

func packetFromC(p C.auris_packet_t) Packet {
	var data []byte
	if p.size > 0 && p.data != nil {
		data = C.GoBytes(unsafe.Pointer(p.data), C.int(p.size))
	}
	return Packet{
		SegmentID:   uint64(p.segment_id),
		PtsUs:       uint64(p.pts_us),
		CaptureTsUs: uint64(p.capture_ts_us),
		Flags:       PacketFlag(p.flags),
		Codec:       Codec(p.codec),
		Data:        data,
	}
}
