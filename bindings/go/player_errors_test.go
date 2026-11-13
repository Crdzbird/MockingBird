// Player error tests: invalid configs are rejected, and Close is idempotent
// with every later call returning ErrClosed.

package mockingbird

import (
	"errors"
	"testing"
	"time"
)

func TestPlayerInvalidConfig(t *testing.T) {
	bad := []PlayerOption{
		WithPlayerRate(44100),
		WithPlayerChannels(3),
		WithTargetDelay(3 * time.Second),
		WithQueuePackets(1),
		WithTargetDelay(-time.Second),
	}
	for i, o := range bad {
		p, err := NewPlayer(WithPlayerBackend(BackendHostPCM), o)
		var se *StatusError
		if !errors.As(err, &se) || se.Status != StatusErrInvalidArg || p != nil {
			t.Errorf("option %d: NewPlayer = %v, %v", i, p, err)
		}
	}
}

func TestPlayerCloseTwice(t *testing.T) {
	p, err := NewPlayer(WithPlayerBackend(BackendHostPCM))
	if err != nil {
		t.Fatal(err)
	}
	if _, err := p.Read(toneFrames); err == nil {
		t.Error("Read before Start succeeded")
	}
	if err := p.Close(); err != nil {
		t.Fatal(err)
	}
	if err := p.Close(); err != nil {
		t.Errorf("second Close: %v", err)
	}
	if err := p.Start(); !errors.Is(err, ErrClosed) {
		t.Errorf("Start after Close = %v", err)
	}
	if _, err := p.Stats(); !errors.Is(err, ErrClosed) {
		t.Errorf("Stats after Close = %v", err)
	}
}
