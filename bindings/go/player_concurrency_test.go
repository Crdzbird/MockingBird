// Player concurrency under -race: one goroutine pushes, another reads stats
// and sets volume and mute, the test goroutine reads PCM, then Close races in.

package mockingbird

import (
	"errors"
	"sync"
	"testing"
)

func TestPlayerConcurrentPushStatsVolume(t *testing.T) {
	const n = 20
	packets := opusTone(t, n)
	p, err := NewPlayer(WithPlayerBackend(BackendHostPCM))
	if err != nil {
		t.Fatal(err)
	}
	if err := p.Start(); err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	wg.Go(func() {
		for round := range 10 {
			for i, data := range packets {
				pts := uint64(round*n+i) * 20_000
				err := p.Push(CodecOpus, data, pts, 0)
				if errors.Is(err, ErrClosed) {
					return
				}
				if err != nil {
					t.Errorf("Push: %v", err)
					return
				}
			}
		}
	})
	wg.Go(func() {
		for i := 0; ; i++ {
			if _, err := p.Stats(); errors.Is(err, ErrClosed) {
				return
			}
			if err := p.SetVolume(float32(i%10) / 10); errors.Is(err, ErrClosed) {
				return
			}
			if err := p.SetMuted(i%2 == 0); errors.Is(err, ErrClosed) {
				return
			}
		}
	})
	for range 200 {
		if _, err := p.Read(toneFrames); err != nil {
			t.Fatal(err)
		}
	}
	if err := p.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
	wg.Wait()
}
