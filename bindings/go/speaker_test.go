package mockingbird

import (
	"path/filepath"
	"testing"
	"time"
)

func TestSpeakerVerificationTellsSpeakersApart(t *testing.T) {
	fixtures := filepath.Join("..", "..", "tests", "fixtures")
	read := func(name string) []int16 {
		s, _, err := readWAV(filepath.Join(fixtures, name))
		if err != nil {
			t.Skipf("fixture unavailable: %v", err)
		}
		return s
	}
	cfg := hostCfg()
	cfg.VAD = VadConfig{Enabled: true, Tier: VadFast, Aggressiveness: 3, HangoverMs: 300}
	cfg.Queues.PCMRingFrames = 1 << 20
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.SetSpeakerModel(filepath.Join("..", "..", "models", "speaker_campplus.onnx")); err != nil {
		t.Fatal(err)
	}
	target, err := e.SpeakerEmbedding(read("speaker_a1.wav"))
	if err != nil {
		t.Skipf("this build has no ONNX Runtime: %v", err)
	}
	if err := e.SetTargetSpeaker(target, 0); err != nil {
		t.Fatal(err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	var verdicts []Event
	collect := func() {
		for {
			select {
			case ev := <-e.Events():
				if ev.Type == EventSpeaker {
					verdicts = append(verdicts, ev)
				}
			case <-e.Packets():
			default:
				return
			}
		}
	}
	audio := append(append(make([]int16, 16000), read("speaker_a2.wav")...), make([]int16, 32000)...)
	audio = append(append(audio, read("speaker_b.wav")...), make([]int16, 32000)...)
	for off := 0; off+320 <= len(audio); off += 320 {
		if err := e.PushPCM(audio[off:off+320], uint64(off)*1_000_000/16000); err != nil {
			t.Fatal(err)
		}
		collect()
	}
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	time.Sleep(50 * time.Millisecond)
	collect()
	if len(verdicts) < 2 || verdicts[0].ReasonMask != 1 || verdicts[len(verdicts)-1].ReasonMask != 0 {
		t.Fatalf("speaker events = %+v, want the first speaker matched and the second not", verdicts)
	}
}
