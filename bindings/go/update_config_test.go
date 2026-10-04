package auris

import (
	"testing"
	"time"
)

func TestUpdateConfigAppliesTunablesAndRejectsStructure(t *testing.T) {
	cfg := hostCfg()
	cfg.VAD.Enabled = true
	cfg.Anomaly.Enabled = true
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	tuned := cfg
	tuned.Anomaly.RMSZ = 1.75
	tuned.Anomaly.FluxZ = 1.75
	tuned.VAD.Aggressiveness = 0
	tuned.VAD.HangoverMs = 500
	if err := e.UpdateConfig(tuned); err != nil {
		t.Fatalf("tunable update rejected: %v", err)
	}
	bad := cfg
	bad.SampleRateHz = 48000
	if err := e.UpdateConfig(bad); err == nil {
		t.Error("expected a structural change to be rejected")
	}
	frame := make([]int16, 320)
	for i := range 10 {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	drainFor(e, 100*time.Millisecond)
	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	if st.FramesCaptured != 3200 {
		t.Errorf("frames_captured = %d, want 3200", st.FramesCaptured)
	}
}

func TestUpdateConfigCannotEnableAMissingDetector(t *testing.T) {
	off := hostCfg()
	e, err := New(off)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	on := off
	on.Anomaly.Enabled = true
	if err := e.UpdateConfig(on); err == nil {
		t.Error("expected enabling a never-constructed detector to be rejected")
	}
}
