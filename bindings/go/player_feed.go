// Player feeding: pushing packets, flushing the buffer and pulling host PCM.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import "unsafe"

// Push copies one packet into the jitter buffer: CodecOpus bytes, or
// CodecPCMS16LE samples at the player's rate and channels, 1 to 8192 bytes
// and at most 120 ms. ptsUs places it on one timeline in microseconds;
// PacketFlagLast ends a talkspurt. Push never blocks; a full buffer drops.
func (p *Player) Push(codec Codec, data []byte, ptsUs uint64, flags PacketFlag) error {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return ErrClosed
	}
	if len(data) == 0 {
		return StatusErrInvalidArg.err("mockingbird_player_push")
	}
	p.feedMu.Lock()
	defer p.feedMu.Unlock()
	st := Status(C.mockingbird_player_push(p.c, C.mockingbird_codec_t(codec),
		(*C.uint8_t)(unsafe.Pointer(&data[0])), C.uint32_t(len(data)),
		C.uint64_t(ptsUs), C.uint32_t(flags)))
	if st != StatusOK {
		return st.err("mockingbird_player_push")
	}
	return nil
}

// PushPacket pushes a Packet from Engine.Packets with its codec, data and
// flags, placed at its CaptureTsUs: one clock for the whole run, where
// PtsUs restarts with every segment.
func (p *Player) PushPacket(pkt Packet) error {
	return p.Push(pkt.Codec, pkt.Data, pkt.CaptureTsUs, pkt.Flags)
}

// Flush drops everything pushed so far and starts a new timeline.
func (p *Player) Flush() error {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return ErrClosed
	}
	p.feedMu.Lock()
	defer p.feedMu.Unlock()
	if st := Status(C.mockingbird_player_flush(p.c)); st != StatusOK {
		return st.err("mockingbird_player_flush")
	}
	return nil
}

// Read renders exactly frames frames, frames * channels interleaved samples,
// with silence where there is nothing to play, and advances playout. Only
// with BackendHostPCM after Start; at most one second per call.
func (p *Player) Read(frames int) ([]int16, error) {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if p.c == nil {
		return nil, ErrClosed
	}
	if frames < 0 || frames > p.rate {
		return nil, StatusErrInvalidArg.err("mockingbird_player_read")
	}
	if frames == 0 {
		return []int16{}, nil
	}
	out := make([]int16, frames*p.channels)
	p.readMu.Lock()
	defer p.readMu.Unlock()
	st := Status(C.mockingbird_player_read(p.c, (*C.int16_t)(unsafe.Pointer(&out[0])), C.uint32_t(frames)))
	if st != StatusOK {
		return nil, st.err("mockingbird_player_read")
	}
	return out, nil
}
