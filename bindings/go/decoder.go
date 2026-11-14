// Decoder handle: creating and releasing a native Opus decoder.

package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

import (
	"runtime"
	"sync"
)

// Decoder turns Opus packets back into interleaved int16 PCM. A stream gets
// its own decoder and its packets go in in order. Methods are safe from any
// goroutine; calls are serialised.
type Decoder struct {
	mu        sync.Mutex
	c         *C.mockingbird_decoder_t
	channels  int
	maxFrames int
	scratch   []int16
}

// NewDecoder creates a decoder that outputs sampleRateHz (8000, 12000,
// 16000, 24000 or 48000) with 1 or 2 interleaved channels. Any Opus packet
// decodes at any of them; a stereo packet is mixed down for a mono decoder.
func NewDecoder(sampleRateHz int, channels int) (*Decoder, error) {
	if sampleRateHz <= 0 || sampleRateHz > 48000 || channels < 1 || channels > 2 {
		return nil, StatusErrInvalidArg.err("mockingbird_decoder_create")
	}
	var c *C.mockingbird_decoder_t
	st := Status(C.mockingbird_decoder_create(C.uint32_t(sampleRateHz), C.uint16_t(channels), &c))
	if st != StatusOK {
		return nil, st.err("mockingbird_decoder_create")
	}
	maxFrames := sampleRateHz / 1000 * 120
	d := &Decoder{c: c, channels: channels, maxFrames: maxFrames, scratch: make([]int16, maxFrames*channels)}
	runtime.SetFinalizer(d, func(d *Decoder) { _ = d.Close() })
	return d, nil
}

// Close releases the native decoder. Idempotent.
func (d *Decoder) Close() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.c == nil {
		return nil
	}
	C.mockingbird_decoder_destroy(d.c)
	d.c = nil
	runtime.SetFinalizer(d, nil)
	return nil
}
