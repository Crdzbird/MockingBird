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
