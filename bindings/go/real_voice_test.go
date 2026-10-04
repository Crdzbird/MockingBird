package auris

import (
	"path/filepath"
	"testing"
	"time"
)

func TestRealVoiceFiresSpeechBegin(t *testing.T) {
	samples, rate, err := readWAV(filepath.Join("..", "..", "tests", "fixtures", "hello.wav"))
	if err != nil || rate != 16000 {
		t.Skipf("fixture unavailable: %v (rate %d)", err, rate)
	}
	cfg := hostCfg()
	cfg.VAD = VadConfig{Enabled: true, Tier: VadFast, Aggressiveness: 1, HangoverMs: 300}
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	onsets := 0
	count := func() {
		for {
			select {
			case ev := <-e.Events():
				if ev.Type == EventSpeechBegin {
					onsets++
				}
			case <-e.Packets():
			default:
				return
			}
		}
	}
	const frameLen = 320
	for off := 0; off+frameLen <= len(samples); off += frameLen {
		if err := e.PushPCM(samples[off:off+frameLen], uint64(off/frameLen)*20_000); err != nil {
			t.Fatal(err)
		}
		if (off/frameLen)%16 == 15 {
			time.Sleep(20 * time.Millisecond)
			count()
		}
	}
	time.Sleep(300 * time.Millisecond)
	count()
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	count()
	if onsets < 1 {
		t.Errorf("real voice produced %d SpeechBegin events, want at least 1", onsets)
	}
}
