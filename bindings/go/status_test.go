package auris

import (
	"errors"
	"testing"
)

func TestInvalidConfigReturnsStatusError(t *testing.T) {
	cfg := hostCfg()
	cfg.SampleRateHz = 12345
	_, err := New(cfg)
	var se *StatusError
	if !errors.As(err, &se) {
		t.Fatalf("error is not *StatusError: %v", err)
	}
	if se.Status != StatusErrInvalidArg || se.Op == "" {
		t.Errorf("got %+v, want StatusErrInvalidArg with an Op", se)
	}
}

func TestVersionAndStatusNames(t *testing.T) {
	if Version() == "" {
		t.Error("Version() is empty")
	}
	if StatusOK.String() == "" || StatusOK.String() == "AURIS_UNKNOWN" {
		t.Errorf("StatusOK string = %q", StatusOK.String())
	}
	if got := Status(123456).String(); got != "AURIS_UNKNOWN" {
		t.Errorf("unknown status string = %q", got)
	}
}

func TestRNNoiseNeedsA48kDivisor(t *testing.T) {
	cfg := hostCfg()
	cfg.Denoise = true
	cfg.SampleRateHz = 32000
	if _, err := New(cfg); err == nil {
		t.Error("expected an error for RNNoise at 32 kHz")
	}
}

func TestStartStopRestart(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	for i := range 5 {
		if err := e.Start(); err != nil {
			t.Fatalf("start %d: %v", i, err)
		}
		_ = e.PushPCM(make([]int16, 320), 0)
		if err := e.Stop(); err != nil {
			t.Fatalf("stop %d: %v", i, err)
		}
	}
}
