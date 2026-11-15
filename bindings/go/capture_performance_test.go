package mockingbird

import "testing"

func TestCapturePerformanceIsAcceptedValidatedAndNotRetuned(t *testing.T) {
	cfg := DefaultConfig()
	if cfg.Performance.Capture != CaptureDefault || cfg.Queues.PCMRingFrames != 1<<15 {
		t.Fatalf("unexpected defaults: %+v %+v", cfg.Performance, cfg.Queues)
	}
	cfg.Backend = BackendHostPCM
	for _, mode := range []CapturePerformance{CaptureDefault, CaptureLowLatency, CapturePowerSaving} {
		cfg.Performance.Capture = mode
		e, err := New(cfg)
		if err != nil {
			t.Fatalf("mode %d refused: %v", mode, err)
		}
		e.Close()
	}
	cfg.Performance.Capture = 3
	if _, err := New(cfg); err == nil {
		t.Fatal("capture performance 3 must be refused")
	}
	cfg.Performance.Capture = CaptureDefault
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	retune := cfg
	retune.Performance.Capture = CaptureLowLatency
	if err := e.UpdateConfig(retune); err == nil {
		t.Fatal("a capture mode change must be refused while live")
	}
	s, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	if s.Capture.Gaps != 0 || s.Capture.GapMs != 0 {
		t.Fatalf("host PCM counted a capture gap: %+v", s.Capture)
	}
}
