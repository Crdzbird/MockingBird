package auris

import (
	"math"
	"testing"
	"time"
)

func TestPCMTapAndLevels(t *testing.T) {
	cfg := hostCfg()
	cfg.Queues.PCMTap = true
	cfg.Queues.PCMTapFrames = 1 << 15
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	const amp, frames = 8192, 10
	frame := squareWave(amp, 2)
	for i := range frames {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	time.Sleep(150 * time.Millisecond)
	got := 0
	buf := make([]int16, 512)
	for {
		n, err := e.ReadPCM(buf)
		if err != nil {
			t.Fatal(err)
		}
		if n == 0 {
			break
		}
		got += n
	}
	if got != frames*len(frame) {
		t.Errorf("tap delivered %d samples, want %d", got, frames*len(frame))
	}
	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	want := float32(20 * math.Log10(float64(amp)/32768.0))
	if math.Abs(float64(st.InputRMSdBFS-want)) > 0.1 || st.TapSamplesDropped != 0 {
		t.Errorf("rms=%v want ~%v, tap dropped=%d", st.InputRMSdBFS, want, st.TapSamplesDropped)
	}
}

func TestReadPCMWithoutTap(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	if _, err := e.ReadPCM(make([]int16, 64)); err == nil {
		t.Error("expected an error when the tap is disabled")
	}
}
