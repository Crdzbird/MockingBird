package mockingbird

import (
	"errors"
	"sync"
	"testing"
	"time"
)

func TestConcurrentUpdateConfig(t *testing.T) {
	cfg := hostCfg()
	cfg.Anomaly.Enabled = true
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for seed := range 4 {
		wg.Go(func() {
			c := cfg
			for i := range 200 {
				c.Anomaly.RMSZ = float32(1.5 + float64((i+seed)%40)*0.1)
				if err := e.UpdateConfig(c); err != nil && !errors.Is(err, ErrClosed) {
					t.Errorf("update: %v", err)
					return
				}
			}
		})
	}
	frame := make([]int16, 320)
	for i := range 100 {
		_ = e.PushPCM(frame, uint64(i)*20_000)
	}
	wg.Wait()
	drainFor(e, 50*time.Millisecond)
}
