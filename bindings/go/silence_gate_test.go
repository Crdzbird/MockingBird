package mockingbird

import (
	"testing"
	"time"
)

func TestSilenceGateSkipsRNNoiseOnQuietFrames(t *testing.T) {
	cfg := hostCfg()
	cfg.Denoise = true
	e, err := New(cfg)
	if err != nil {
		t.Skipf("RNNoise not compiled in: %v", err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	silent := make([]int16, 320)
	loud := squareWave(3000, 32)
	var ts uint64
	push := func(f []int16, n int) {
		for range n {
			if err := e.PushPCM(f, ts); err != nil {
				t.Fatal(err)
			}
			ts += 20_000
		}
		time.Sleep(150 * time.Millisecond)
	}
	skipped := func() uint64 {
		st, err := e.Stats()
		if err != nil {
			t.Fatal(err)
		}
		if st.Worker.DegradationLevel != 0 {
			t.Fatalf("degradation level = %d with adaptive off", st.Worker.DegradationLevel)
		}
		return st.Worker.PreprocFramesSkipped
	}
	push(silent, 25)
	if got := skipped(); got != 25 {
		t.Fatalf("skipped after silence = %d, want 25", got)
	}
	push(loud, 25)
	if got := skipped(); got != 25 {
		t.Fatalf("skipped after signal = %d, want 25", got)
	}
	cfg.Performance.SilenceGateDbfs = -120
	if err := e.UpdateConfig(cfg); err != nil {
		t.Fatal(err)
	}
	push(silent, 10)
	if got := skipped(); got != 25 {
		t.Fatalf("skipped after disabling the gate = %d, want 25", got)
	}
}
