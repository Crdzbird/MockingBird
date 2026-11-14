package mockingbird

import (
	"errors"
	"testing"
)

func TestOpusRejectsThirtyMsFrames(t *testing.T) {
	for _, enc := range []EncoderConfig{OpusEncoder(), OggOpusEncoder()} {
		cfg := hostCfg()
		cfg.FrameMs = 30
		cfg.Encoder = enc
		e, err := New(cfg)
		var se *StatusError
		if !errors.As(err, &se) || se.Status != StatusErrUnsupported {
			t.Errorf("codec %v at 30 ms: got %v, want StatusErrUnsupported", enc.Codec, err)
		}
		if e != nil {
			t.Errorf("codec %v at 30 ms: got an engine", enc.Codec)
		}
	}
}

func TestPCMKeepsThirtyMsFrames(t *testing.T) {
	cfg := hostCfg()
	cfg.FrameMs = 30
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
}
