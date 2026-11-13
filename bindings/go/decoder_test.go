// Decoder tests: a round trip through the engine's encoder, concealment
// lengths, garbage input and use after Close.

package mockingbird

import (
	"errors"
	"testing"
)

func newDecoder(t *testing.T, rate, channels int) *Decoder {
	t.Helper()
	d, err := NewDecoder(rate, channels)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = d.Close() })
	return d
}

func TestDecoderRoundTrip(t *testing.T) {
	const n = 25
	packets := opusTone(t, n)
	d := newDecoder(t, 16000, 1)
	var total, sumAbs int
	for _, pkt := range packets {
		pcm, err := d.Decode(pkt)
		if err != nil {
			t.Fatal(err)
		}
		total += len(pcm)
		for _, s := range pcm {
			sumAbs += max(int(s), -int(s))
		}
	}
	if total != n*toneFrames {
		t.Errorf("decoded %d samples, want %d", total, n*toneFrames)
	}
	if mean := sumAbs / max(total, 1); mean < 1000 {
		t.Errorf("decoded audio mean |s| = %d, want a tone", mean)
	}
}

func TestDecoderConceal(t *testing.T) {
	cases := []struct{ rate, channels, frames, want int }{
		{16000, 1, 0, 320},
		{16000, 1, 160, 160},
		{48000, 2, 960, 1920},
	}
	for _, c := range cases {
		pcm, err := newDecoder(t, c.rate, c.channels).Conceal(c.frames)
		if err != nil {
			t.Fatal(err)
		}
		if len(pcm) != c.want {
			t.Errorf("Conceal(%d) at %d/%d = %d samples, want %d", c.frames, c.rate, c.channels, len(pcm), c.want)
		}
	}
}

func TestDecoderRejectsGarbage(t *testing.T) {
	d := newDecoder(t, 16000, 1)
	for _, pkt := range [][]byte{{0xff, 0xff, 0xff}, nil} {
		_, err := d.Decode(pkt)
		var se *StatusError
		if !errors.As(err, &se) || se.Status != StatusErrInvalidArg {
			t.Errorf("Decode(%x) = %v, want invalid arg", pkt, err)
		}
	}
	if _, err := NewDecoder(44100, 1); err == nil {
		t.Error("NewDecoder(44100, 1) succeeded")
	}
	if err := d.Reset(); err != nil {
		t.Errorf("Reset: %v", err)
	}
}

func TestDecoderCloseTwice(t *testing.T) {
	d, err := NewDecoder(16000, 1)
	if err != nil {
		t.Fatal(err)
	}
	if err := d.Close(); err != nil {
		t.Fatal(err)
	}
	if err := d.Close(); err != nil {
		t.Errorf("second Close: %v", err)
	}
	if _, err := d.Decode([]byte{1}); !errors.Is(err, ErrClosed) {
		t.Errorf("Decode after Close = %v", err)
	}
}
