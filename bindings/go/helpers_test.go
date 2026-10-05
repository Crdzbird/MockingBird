package mockingbird

import (
	"encoding/binary"
	"os"
	"testing"
	"time"
)

func hostCfg() Config {
	c := DefaultConfig()
	c.Backend = BackendHostPCM
	c.VAD.Enabled = false
	c.Anomaly.Enabled = false
	return c
}

func squareWave(amp int16, period int) []int16 {
	frame := make([]int16, 320)
	for i := range frame {
		if i%period < period/2 {
			frame[i] = amp
		} else {
			frame[i] = -amp
		}
	}
	return frame
}

func drainFor(e *Engine, d time.Duration) {
	deadline := time.After(d)
	for {
		select {
		case <-e.Packets():
		case <-e.Events():
		case <-deadline:
			return
		}
	}
}

func readWAV(path string) (samples []int16, rate uint32, err error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, 0, err
	}
	if len(b) < 12 || string(b[0:4]) != "RIFF" || string(b[8:12]) != "WAVE" {
		return nil, 0, os.ErrInvalid
	}
	for i := 12; i+8 <= len(b); {
		id := string(b[i : i+4])
		sz := int(binary.LittleEndian.Uint32(b[i+4 : i+8]))
		body := b[i+8:]
		if len(body) < sz {
			break
		}
		switch id {
		case "fmt ":
			rate = binary.LittleEndian.Uint32(body[4:8])
		case "data":
			samples = make([]int16, sz/2)
			for k := range samples {
				samples[k] = int16(binary.LittleEndian.Uint16(body[k*2 : k*2+2]))
			}
			return samples, rate, nil
		}
		i += 8 + sz + sz%2
	}
	return nil, 0, os.ErrInvalid
}

func closeOnExit(t *testing.T, e *Engine) {
	t.Helper()
	t.Cleanup(func() {
		if err := e.Close(); err != nil {
			t.Errorf("Close: %v", err)
		}
	})
}
