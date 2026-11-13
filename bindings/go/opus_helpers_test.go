// Test helpers: Opus packets of a tone from a HOST_PCM engine, and a player
// with the HOST_PCM backend that plays them through Read.

package mockingbird

import (
	"math"
	"testing"
)

const toneFrames = 320

func toneFrame(i int) []int16 {
	frame := make([]int16, toneFrames)
	for k := range frame {
		phase := 2 * math.Pi * 440 * float64(i*toneFrames+k) / 16000
		frame[k] = int16(8000 * math.Sin(phase))
	}
	return frame
}

func opusTone(t *testing.T, n int) [][]byte {
	t.Helper()
	cfg := hostCfg()
	cfg.Encoder = OpusEncoder()
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
	for i := range n {
		if err := e.PushPCM(toneFrame(i), uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	var out [][]byte
	for done := false; !done; {
		select {
		case pkt := <-e.Packets():
			if pkt.Codec != CodecOpus {
				t.Fatalf("packet codec = %v", pkt.Codec)
			}
			out = append(out, pkt.Data)
		case <-e.Events():
		default:
			done = true
		}
	}
	if len(out) != n {
		t.Fatalf("engine produced %d Opus packets, want %d", len(out), n)
	}
	return out
}

func hostPlayer(t *testing.T, opts ...PlayerOption) *Player {
	t.Helper()
	p, err := NewPlayer(append([]PlayerOption{WithPlayerBackend(BackendHostPCM)}, opts...)...)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if err := p.Close(); err != nil {
			t.Errorf("Close: %v", err)
		}
	})
	return p
}

func pushTone(t *testing.T, p *Player, packets [][]byte, skip int) {
	t.Helper()
	for i, data := range packets {
		var flags PacketFlag
		if i == len(packets)-1 {
			flags = PacketFlagLast
		}
		if i == skip {
			continue
		}
		if err := p.Push(CodecOpus, data, uint64(i)*20_000, flags); err != nil {
			t.Fatal(err)
		}
	}
}
