package mockingbird

import (
	"path/filepath"
	"testing"
	"time"
)

func TestKeywordFiresOnceAboveItsThreshold(t *testing.T) {
	fixtures := filepath.Join("..", "..", "tests", "fixtures")
	assets := filepath.Join("..", "..", "packages", "mockingbird", "assets")
	samples, rate, err := readWAV(filepath.Join(fixtures, "hello.wav"))
	if err != nil || rate != 16000 {
		t.Skipf("fixture unavailable: %v (rate %d)", err, rate)
	}
	cfg := hostCfg()
	cfg.Queues.PCMRingFrames = 1 << 18
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	closeOnExit(t, e)
	if err := e.SetKeywordFrontend(filepath.Join(assets, "melspectrogram.onnx"),
		filepath.Join(assets, "embedding_model.onnx")); err != nil {
		t.Fatal(err)
	}
	id, err := e.AddKeyword(filepath.Join(fixtures, "keyword_test.onnx"), 0.9)
	if err != nil || id != 0 {
		t.Fatalf("AddKeyword = %d, %v", id, err)
	}
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}
	if _, err := e.AddKeyword(filepath.Join(fixtures, "keyword_test.onnx"), 0.5); err == nil {
		t.Error("AddKeyword while running succeeded")
	}
	var hits []Event
	collect := func() {
		for {
			select {
			case ev := <-e.Events():
				if ev.Type == EventBackendError {
					t.Skip("this build has no ONNX Runtime")
				}
				if ev.Type == EventKeyword {
					hits = append(hits, ev)
				}
			case <-e.Packets():
			default:
				return
			}
		}
	}
	for off := 0; off+320 <= len(samples); off += 320 {
		if err := e.PushPCM(samples[off:off+320], uint64(off)*1_000_000/16000); err != nil {
			t.Fatal(err)
		}
		collect()
	}
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	time.Sleep(50 * time.Millisecond)
	collect()
	if len(hits) != 1 || hits[0].ReasonMask != 0 || hits[0].Confidence < 0.9 {
		t.Fatalf("keyword events = %+v, want one with id 0 above 0.9", hits)
	}
}
