package auris

import (
	"sync"
	"testing"
	"time"
)

// Close must return promptly even when no one is draining the Events/Packets
// channels and they have filled — the drain goroutine must never block on a
// send. (Regression for the blocking-send deadlock.)
func TestCloseDoesNotDeadlockWithFullChannels(t *testing.T) {
	cfg := hostCfg()
	cfg.EnableVAD = false
	cfg.EnableAnomaly = false
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	// Produce far more packets than the 256-deep channel can hold, never
	// reading it. Pace the pushes so the worker keeps making packets (rather
	// than the ring dropping everything).
	frame := make([]int16, 320)
	for i := 0; i < 600; i++ {
		_ = e.PushPCM(frame, uint64(i)*20_000)
		if i%16 == 15 {
			time.Sleep(3 * time.Millisecond)
		}
	}
	time.Sleep(50 * time.Millisecond) // give the pump time to fill + start dropping

	closed := make(chan error, 1)
	go func() { closed <- e.Close() }()
	select {
	case err := <-closed:
		if err != nil {
			t.Fatalf("Close: %v", err)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("Close deadlocked with full channels")
	}

	if _, dp := e.Dropped(); dp == 0 {
		t.Log("note: expected some packet drops, got 0 (timing-dependent, not fatal)")
	}
}

// Stats/PushPCM must not use-after-free when Close races them. The RWMutex must
// keep the native pointer alive for the duration of each native call. Run with
// -race to catch the UAF.
func TestConcurrentStatsAndClose(t *testing.T) {
	cfg := hostCfg()
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			frame := make([]int16, 320)
			for {
				if _, err := e.Stats(); err == ErrClosed {
					return
				}
				if err := e.PushPCM(frame, 0); err == ErrClosed {
					return
				}
			}
		}()
	}
	time.Sleep(20 * time.Millisecond)
	if err := e.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
	wg.Wait() // all callers observed ErrClosed cleanly, no crash/UAF
}

// Many goroutines pushing concurrently must not corrupt the single-producer
// PCM ring (pushMu serializes them). Run with -race.
func TestConcurrentPushPCM(t *testing.T) {
	cfg := hostCfg()
	cfg.EnableVAD = false
	cfg.EnableAnomaly = false
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	// Drain in the background so the pipeline keeps flowing.
	stop := make(chan struct{})
	go func() {
		for {
			select {
			case <-e.Packets():
			case <-e.Events():
			case <-stop:
				return
			}
		}
	}()

	var wg sync.WaitGroup
	const producers, each = 8, 200
	for p := 0; p < producers; p++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			frame := make([]int16, 320)
			for i := 0; i < each; i++ {
				if err := e.PushPCM(frame, 0); err != nil && err != ErrClosed {
					t.Errorf("push: %v", err)
					return
				}
			}
		}()
	}
	wg.Wait()
	time.Sleep(50 * time.Millisecond)
	close(stop)

	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	// Everything captured-or-dropped must be accounted for; nothing invented.
	total := uint64(producers * each * 320)
	if st.FramesCaptured+st.FramesDroppedCapture > total {
		t.Errorf("captured(%d)+dropped(%d) > pushed(%d)",
			st.FramesCaptured, st.FramesDroppedCapture, total)
	}
}

// All methods on a closed engine return cleanly (ErrClosed / no-op), never panic.
func TestUseAfterClose(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Close(); err != nil {
		t.Fatal(err)
	}
	if err := e.Close(); err != nil { // idempotent
		t.Errorf("double Close: %v", err)
	}
	if err := e.Start(); err != ErrClosed {
		t.Errorf("Start after Close = %v, want ErrClosed", err)
	}
	if err := e.Stop(); err != ErrClosed {
		t.Errorf("Stop after Close = %v, want ErrClosed", err)
	}
	if err := e.PushPCM([]int16{1, 2, 3}, 0); err != ErrClosed {
		t.Errorf("PushPCM after Close = %v, want ErrClosed", err)
	}
	if _, err := e.Stats(); err != ErrClosed {
		t.Errorf("Stats after Close = %v, want ErrClosed", err)
	}
}

// Stop must flush the worker's tail packets to the channels — pushing N frames
// in passthrough and stopping immediately must yield exactly N packets, none
// lost. (Regression for stopping the pump before the native flush.)
func TestStopFlushesTailPackets(t *testing.T) {
	cfg := hostCfg()
	cfg.EnableVAD = false
	cfg.EnableAnomaly = false
	cfg.PcmRingFrames = 1 << 17    // hold the whole push without dropping
	cfg.PacketQueueCapacity = 4096 // and every resulting packet
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	const n = 100
	frame := make([]int16, 320)
	for i := 0; i < n; i++ {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	// Stop immediately (no wait): the flush must still deliver every packet.
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}

	got := 0
	for {
		select {
		case <-e.Packets():
			got++
		case <-e.Events():
		default:
			goto done
		}
	}
done:
	if got != n {
		t.Errorf("got %d packets after Stop, want %d (tail lost)", got, n)
	}
}

// UpdateConfig retunes in place: tunables apply, structural changes are
// rejected, and the engine keeps running.
func TestUpdateConfig(t *testing.T) {
	cfg := hostCfg()
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	tuned := cfg
	tuned.AnomalyRMSZ = 1.75
	tuned.AnomalyFluxZ = 1.75
	tuned.VadAggressiveness = 0
	tuned.VadHangoverMs = 500
	if err := e.UpdateConfig(tuned); err != nil {
		t.Fatalf("tunable update rejected: %v", err)
	}

	// Structural change must be refused.
	bad := cfg
	bad.SampleRateHz = 48000
	if err := e.UpdateConfig(bad); err == nil {
		t.Error("expected structural change to be rejected")
	}

	// Enabling a detector that was off at create time must be refused.
	off := hostCfg()
	off.EnableAnomaly = false
	e2, err := New(off)
	if err != nil {
		t.Fatal(err)
	}
	defer e2.Close()
	if err := e2.Start(); err != nil {
		t.Fatal(err)
	}
	on := off
	on.EnableAnomaly = true
	if err := e2.UpdateConfig(on); err == nil {
		t.Error("expected enabling a never-constructed detector to be rejected")
	}

	// Still functional after the retune.
	frame := make([]int16, 320)
	for i := 0; i < 10; i++ {
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

// Concurrent UpdateConfig from many goroutines must be race-free (-race).
func TestConcurrentUpdateConfig(t *testing.T) {
	cfg := hostCfg()
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	var wg sync.WaitGroup
	for g := 0; g < 4; g++ {
		wg.Add(1)
		go func(seed int) {
			defer wg.Done()
			c := cfg
			for i := 0; i < 200; i++ {
				c.AnomalyRMSZ = float32(1.5 + float64((i+seed)%40)*0.1)
				if err := e.UpdateConfig(c); err != nil && err != ErrClosed {
					t.Errorf("update: %v", err)
					return
				}
			}
		}(g)
	}
	frame := make([]int16, 320)
	for i := 0; i < 100; i++ {
		_ = e.PushPCM(frame, uint64(i)*20_000)
	}
	wg.Wait()
	drainFor(e, 50*time.Millisecond)
}

// Stop then Start again must work (engine restart).
func TestStartStopRestart(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	for i := 0; i < 5; i++ {
		if err := e.Start(); err != nil {
			t.Fatalf("start %d: %v", i, err)
		}
		_ = e.PushPCM(make([]int16, 320), 0)
		if err := e.Stop(); err != nil {
			t.Fatalf("stop %d: %v", i, err)
		}
	}
}
