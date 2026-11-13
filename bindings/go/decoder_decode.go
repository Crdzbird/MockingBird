// Decoder audio: decoding packets, concealing lost ones and resetting state.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import "unsafe"

const maxOpusPacketBytes = 65536

// Decode decodes one Opus packet of 1 to 65536 bytes and returns its frames,
// frames * channels interleaved samples. A packet libopus cannot parse is a
// StatusErrInvalidArg error and leaves the decoder usable.
func (d *Decoder) Decode(packet []byte) ([]int16, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.c == nil {
		return nil, ErrClosed
	}
	if len(packet) == 0 || len(packet) > maxOpusPacketBytes {
		return nil, StatusErrInvalidArg.err("mockingbird_decoder_decode")
	}
	out := make([]int16, d.maxFrames*d.channels)
	var n C.uint32_t
	st := Status(C.mockingbird_decoder_decode(d.c,
		(*C.uint8_t)(unsafe.Pointer(&packet[0])), C.uint32_t(len(packet)),
		(*C.int16_t)(unsafe.Pointer(&out[0])), C.uint32_t(d.maxFrames), &n))
	if st != StatusOK {
		return nil, st.err("mockingbird_decoder_decode")
	}
	return out[:int(n)*d.channels], nil
}

// Conceal synthesises audio for one missing packet so the next one continues
// smoothly. frames is a multiple of 2.5 ms and at most 120 ms; 0 means the
// duration of the last decoded packet, or 20 ms before the first one.
func (d *Decoder) Conceal(frames int) ([]int16, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.c == nil {
		return nil, ErrClosed
	}
	if frames < 0 || frames > d.maxFrames {
		return nil, StatusErrInvalidArg.err("mockingbird_decoder_conceal")
	}
	out := make([]int16, d.maxFrames*d.channels)
	var n C.uint32_t
	st := Status(C.mockingbird_decoder_conceal(d.c, C.uint32_t(frames),
		(*C.int16_t)(unsafe.Pointer(&out[0])), C.uint32_t(d.maxFrames), &n))
	if st != StatusOK {
		return nil, st.err("mockingbird_decoder_conceal")
	}
	return out[:int(n)*d.channels], nil
}

// Reset forgets the decoder's history, for a new stream on the same decoder.
func (d *Decoder) Reset() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.c == nil {
		return ErrClosed
	}
	if st := Status(C.mockingbird_decoder_reset(d.c)); st != StatusOK {
		return st.err("mockingbird_decoder_reset")
	}
	return nil
}
