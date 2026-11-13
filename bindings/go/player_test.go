// Player playout tests over the HOST_PCM backend: every packet played, and
// a dropped packet concealed.

package mockingbird

import (
	"testing"
	"time"
)

func playOut(t *testing.T, p *Player, n int) (PlayerStats, int16) {
	t.Helper()
	if err := p.Start(); err != nil {
		t.Fatal(err)
	}
	var peak int16
	for range n + 30 {
		pcm, err := p.Read(toneFrames)
		if err != nil {
			t.Fatal(err)
		}
		for _, s := range pcm {
			peak = max(peak, s)
		}
		st, err := p.Stats()
		if err != nil {
			t.Fatal(err)
		}
		if st.State == PlayerStateIdle && st.PacketsPlayed > 0 {
			return st, peak
		}
	}
	st, err := p.Stats()
	if err != nil {
		t.Fatal(err)
	}
	t.Fatalf("player never went idle: %+v", st)
	return st, peak
}

func TestPlayerPlaysEveryPacket(t *testing.T) {
	const n = 15
	p := hostPlayer(t, WithMaxDelay(time.Second))
	pushTone(t, p, opusTone(t, n), -1)
	st, peak := playOut(t, p, n)
	if st.PacketsPlayed != n || st.PacketsConcealed != 0 || st.PacketsDropped != 0 {
		t.Errorf("stats = %+v", st)
	}
	if peak < 1000 {
		t.Errorf("output peak = %d, want a tone", peak)
	}
	if st.State.String() != "idle" {
		t.Errorf("state = %v", st.State)
	}
}

func TestPlayerConcealsDroppedPacket(t *testing.T) {
	const n = 15
	p := hostPlayer(t, WithMaxDelay(time.Second))
	pushTone(t, p, opusTone(t, n), 7)
	st, _ := playOut(t, p, n)
	if st.PacketsPlayed != n-1 || st.PacketsConcealed != 1 {
		t.Errorf("stats = %+v", st)
	}
}
