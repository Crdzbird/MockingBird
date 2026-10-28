package mockingbird

import (
	"bytes"
	"encoding/binary"
	"testing"
)

func TestWAVHeader(t *testing.T) {
	pcm := []byte{1, 0, 2, 0, 3, 0, 4, 0}
	w := WAV(pcm, 16000, 1)
	le := binary.LittleEndian
	if len(w) != 52 || string(w[0:4]) != "RIFF" || string(w[8:16]) != "WAVEfmt " ||
		string(w[36:40]) != "data" {
		t.Fatalf("bad layout: % x", w[:44])
	}
	if le.Uint32(w[4:]) != 44 || le.Uint32(w[24:]) != 16000 || le.Uint32(w[28:]) != 32000 ||
		le.Uint16(w[32:]) != 2 || le.Uint16(w[34:]) != 16 || le.Uint32(w[40:]) != 8 {
		t.Fatalf("bad fields: % x", w[:44])
	}
	if !bytes.Equal(w[44:], pcm) {
		t.Fatal("samples not copied")
	}
	if CodecOggOpus != 3 || CodecOggOpus.String() != "ogg-opus" || OggOpusEncoder().Codec != CodecOggOpus {
		t.Fatal("ogg opus codec mismatch")
	}
}
