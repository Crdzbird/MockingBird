package auris

import (
	"errors"
	"sync"
	"testing"
	"time"
)

func TestConcurrentStatsAndClose(t *testing.T) {
	cfg := hostCfg()
	cfg.VAD.Enabled = true
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for range 8 {
		wg.Go(func() {
			frame := make([]int16, 320)
			for {
				if _, err := e.Stats(); errors.Is(err, ErrClosed) {
					return
				}
				if err := e.PushPCM(frame, 0); errors.Is(err, ErrClosed) {
					return
				}
			}
		})
	}
	time.Sleep(20 * time.Millisecond)
	if err := e.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
	wg.Wait()
}

func TestConcurrentPushPCM(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
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
	const producers, each = 8, 200
	var wg sync.WaitGroup
	for range producers {
		wg.Go(func() {
			frame := make([]int16, 320)
			for range each {
				if err := e.PushPCM(frame, 0); err != nil && !errors.Is(err, ErrClosed) {
					t.Errorf("push: %v", err)
					return
				}
			}
		})
	}
	wg.Wait()
	time.Sleep(50 * time.Millisecond)
	close(stop)
	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	if total := uint64(producers * each * 320); st.FramesCaptured+st.FramesDroppedCapture > total {
		t.Errorf("captured(%d)+dropped(%d) > pushed(%d)", st.FramesCaptured, st.FramesDroppedCapture, total)
	}
}
