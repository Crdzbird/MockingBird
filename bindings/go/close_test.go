package mockingbird

import (
	"errors"
	"testing"
	"time"
)

func TestCloseDoesNotDeadlockWithFullChannels(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	frame := make([]int16, 320)
	for i := range 600 {
		_ = e.PushPCM(frame, uint64(i)*20_000)
		if i%16 == 15 {
			time.Sleep(3 * time.Millisecond)
		}
	}
	time.Sleep(50 * time.Millisecond)
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
}

func TestUseAfterClose(t *testing.T) {
	e, err := New(hostCfg())
	if err != nil {
		t.Fatal(err)
	}
	if err := e.Close(); err != nil {
		t.Fatal(err)
	}
	if err := e.Close(); err != nil {
		t.Errorf("second Close: %v", err)
	}
	if err := e.Start(); !errors.Is(err, ErrClosed) {
		t.Errorf("Start after Close = %v", err)
	}
	if err := e.Stop(); !errors.Is(err, ErrClosed) {
		t.Errorf("Stop after Close = %v", err)
	}
	if err := e.PushPCM([]int16{1, 2, 3}, 0); !errors.Is(err, ErrClosed) {
		t.Errorf("PushPCM after Close = %v", err)
	}
	if _, err := e.Stats(); !errors.Is(err, ErrClosed) {
		t.Errorf("Stats after Close = %v", err)
	}
}

func TestStopFlushesTailPackets(t *testing.T) {
	cfg := hostCfg()
	cfg.Queues.PCMRingFrames = 1 << 17
	cfg.Queues.PacketQueueCapacity = 4096
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	const n = 100
	frame := make([]int16, 320)
	for i := range n {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	got := 0
	for done := false; !done; {
		select {
		case <-e.Packets():
			got++
		case <-e.Events():
		default:
			done = true
		}
	}
	if got != n {
		t.Errorf("got %d packets after Stop, want %d", got, n)
	}
}
