package auris

import (
	"testing"
	"time"
)

func TestHostPCMPassthrough(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	const n = 20
	frame := make([]int16, 320)
	for i := range n {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	got := 0
	deadline := time.After(time.Second)
	for got < n {
		select {
		case pkt := <-e.Packets():
			if pkt.Codec != CodecPCMS16LE {
				t.Errorf("packet codec = %v", pkt.Codec)
			}
			got++
		case <-deadline:
			t.Fatalf("passthrough produced %d packets, want %d", got, n)
		}
	}
}

func TestStatsAccounting(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	const n = 30
	frame := make([]int16, 320)
	for range n {
		if err := e.PushPCM(frame, 0); err != nil {
			t.Fatal(err)
		}
	}
	drainFor(e, 150*time.Millisecond)
	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	if st.FramesCaptured != n*320 || st.FramesDroppedCapture != 0 {
		t.Errorf("captured=%d dropped=%d", st.FramesCaptured, st.FramesDroppedCapture)
	}
}

func TestRepeatedLifecycle(t *testing.T) {
	for i := range 10 {
		cfg := hostCfg()
		cfg.VAD.Enabled = i%2 == 0
		cfg.Anomaly.Enabled = i%2 == 1
		e, err := New(cfg)
		if err != nil {
			t.Fatal(err)
		}
		if err := e.Start(); err != nil {
			t.Fatal(err)
		}
		frame := make([]int16, 320)
		for k := range 5 {
			_ = e.PushPCM(frame, uint64(k)*20_000)
		}
		drainFor(e, 20*time.Millisecond)
		if err := e.Stop(); err != nil {
			t.Fatal(err)
		}
		if err := e.Close(); err != nil {
			t.Fatal(err)
		}
	}
}
