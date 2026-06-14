package auris

import (
	"encoding/binary"
	"errors"
	"math"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestStatusError(t *testing.T) {
	cfg := hostCfg()
	cfg.SampleRateHz = 12345 // invalid
	_, err := New(cfg)
	if err == nil {
		t.Fatal("expected an error")
	}
	var se *StatusError
	if !errors.As(err, &se) {
		t.Fatalf("error is not *StatusError: %v", err)
	}
	if se.Status != StatusErrInvalidArg {
		t.Errorf("Status = %v, want StatusErrInvalidArg", se.Status)
	}
	if se.Op == "" {
		t.Error("StatusError.Op is empty")
	}
}

// ---------------------------------------------------------------------------
// Pure-Go helpers (no native engine).
// ---------------------------------------------------------------------------

func TestVersionAndStatus(t *testing.T) {
	if Version() == "" {
		t.Error("Version() is empty")
	}
	if StatusOK.String() == "AURIS_UNKNOWN" || StatusOK.String() == "" {
		t.Errorf("StatusOK string = %q", StatusOK.String())
	}
	if got := Status(123456).String(); got != "AURIS_UNKNOWN" {
		t.Errorf("unknown status string = %q, want AURIS_UNKNOWN", got)
	}
}

func TestCategoryMapping(t *testing.T) {
	speech := []int{0, 1, 2, 3, 4, 6, 9, 12} // incl. Conversation(2)/Narration(3) — non-English landing spots
	for _, c := range speech {
		if CategoryForYAMNetClass(c) != CategorySpeech || !IsSpeechClass(c) {
			t.Errorf("class %d should be speech", c)
		}
	}
	for _, c := range []int{67, 69, 70, 76, 131} {
		if CategoryForYAMNetClass(c) != CategoryAnimal {
			t.Errorf("class %d should be animal", c)
		}
	}
	for _, c := range []int{132, 200, 282} {
		if CategoryForYAMNetClass(c) != CategoryMusic {
			t.Errorf("class %d should be music", c)
		}
	}
	for _, c := range []int{494, 300, -1, 99999} {
		if CategoryForYAMNetClass(c) != CategoryOther {
			t.Errorf("class %d should be other", c)
		}
	}
	// IsHumanVoice covers speech + laughter + crying + singing (0..34).
	for _, c := range []int{0, 2, 12, 13, 24, 34} {
		if !IsHumanVoice(c) {
			t.Errorf("class %d should be human voice", c)
		}
	}
	for _, c := range []int{35, 36, 48, 67, 132, 494, -1} {
		if IsHumanVoice(c) {
			t.Errorf("class %d should NOT be human voice", c)
		}
	}
}

func approx(a, b float32) bool { return math.Abs(float64(a-b)) < 1e-4 }

func TestSensitiveDial(t *testing.T) {
	low := Sensitive(0)
	high := Sensitive(1)

	if !(low.VadNeuralThreshold > high.VadNeuralThreshold) {
		t.Error("lower sensitivity should give a higher (stricter) VAD threshold")
	}
	if !approx(low.VadNeuralThreshold, 0.80) || !approx(high.VadNeuralThreshold, 0.20) {
		t.Errorf("VAD threshold endpoints: low=%v high=%v", low.VadNeuralThreshold, high.VadNeuralThreshold)
	}
	if low.VadAggressiveness != 3 || high.VadAggressiveness != 0 {
		t.Errorf("aggressiveness: low=%d high=%d", low.VadAggressiveness, high.VadAggressiveness)
	}
	if !approx(low.AnomalyRMSZ, 5.0) || !approx(high.AnomalyRMSZ, 1.5) {
		t.Errorf("anomaly z endpoints: low=%v high=%v", low.AnomalyRMSZ, high.AnomalyRMSZ)
	}
	// Out-of-range is clamped.
	if Sensitive(5).VadNeuralThreshold != Sensitive(1).VadNeuralThreshold {
		t.Error("sensitivity > 1 not clamped")
	}
	if Sensitive(-5).VadNeuralThreshold != Sensitive(0).VadNeuralThreshold {
		t.Error("sensitivity < 0 not clamped")
	}
	// Defaults: neural VAD, all detectors on.
	c := Sensitive(0.5)
	if c.VadTier != VadNeural || !c.EnableVAD || !c.EnableAnomaly || !c.EnableSoundClass {
		t.Error("Sensitive defaults should be neural VAD with all detectors enabled")
	}
	// Options apply.
	c2 := Sensitive(0.5, WithBackend(BackendHostPCM), WithRNNoise(true), WithVadTier(VadFast))
	if c2.Backend != BackendHostPCM || !c2.EnableRNNoise || c2.VadTier != VadFast {
		t.Error("Sensitive options not applied")
	}
}

// ---------------------------------------------------------------------------
// Native engine.
// ---------------------------------------------------------------------------

func hostCfg() Config {
	c := DefaultConfig()
	c.Backend = BackendHostPCM
	c.Codec = CodecPCMS16LE
	return c
}

func TestConfigValidation(t *testing.T) {
	cfg := hostCfg()
	cfg.SampleRateHz = 12345 // not a supported rate
	if _, err := New(cfg); err == nil {
		t.Error("expected error for invalid sample rate")
	}

	cfg = hostCfg()
	cfg.EnableRNNoise = true
	cfg.SampleRateHz = 32000 // RNNoise needs an integer 48 kHz ratio
	if _, err := New(cfg); err == nil {
		t.Error("expected error for RNNoise @ 32 kHz")
	}

	cfg = hostCfg()
	e, err := New(cfg)
	if err != nil {
		t.Fatalf("valid config rejected: %v", err)
	}
	if err := e.Close(); err != nil {
		t.Errorf("close: %v", err)
	}
}

func TestHostPCMPassthrough(t *testing.T) {
	cfg := hostCfg()
	cfg.EnableVAD = false
	cfg.EnableAnomaly = false
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	const n = 20
	frame := make([]int16, 320)
	for i := 0; i < n; i++ {
		if err := e.PushPCM(frame, uint64(i)*20_000); err != nil {
			t.Fatal(err)
		}
	}

	got := 0
	deadline := time.After(time.Second)
loop:
	for got < n {
		select {
		case pkt := <-e.Packets():
			if pkt.Codec != CodecPCMS16LE {
				t.Errorf("packet codec = %v", pkt.Codec)
			}
			got++
		case <-deadline:
			break loop
		}
	}
	if got != n {
		t.Errorf("passthrough produced %d packets, want %d", got, n)
	}
}

func TestStatsAccounting(t *testing.T) {
	cfg := hostCfg()
	cfg.EnableVAD = false
	cfg.EnableAnomaly = false
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	const n = 30
	frame := make([]int16, 320)
	for i := 0; i < n; i++ {
		if err := e.PushPCM(frame, 0); err != nil {
			t.Fatal(err)
		}
	}
	drainFor(e, 150*time.Millisecond)

	st, err := e.Stats()
	if err != nil {
		t.Fatal(err)
	}
	if st.FramesCaptured != n*320 {
		t.Errorf("frames_captured = %d, want %d", st.FramesCaptured, n*320)
	}
	if st.FramesDroppedCapture != 0 {
		t.Errorf("frames_dropped_capture = %d, want 0", st.FramesDroppedCapture)
	}
}

func TestRepeatedLifecycle(t *testing.T) {
	// Create/start/stop/close churn must not leak or panic.
	for i := 0; i < 10; i++ {
		cfg := hostCfg()
		cfg.EnableVAD = i%2 == 0
		cfg.EnableAnomaly = i%2 == 1
		e, err := New(cfg)
		if err != nil {
			t.Fatal(err)
		}
		if err := e.Start(); err != nil {
			t.Fatal(err)
		}
		frame := make([]int16, 320)
		for k := 0; k < 5; k++ {
			e.PushPCM(frame, uint64(k)*20_000)
		}
		drainFor(e, 20*time.Millisecond)
		if err := e.Stop(); err != nil {
			t.Fatal(err)
		}
		if err := e.Close(); err != nil {
			t.Fatal(err)
		}
	}
}

// Differential: a real speech fixture must fire at least one SpeechBegin.
func TestRealVoice(t *testing.T) {
	path := filepath.Join("..", "..", "tests", "fixtures", "hello.wav")
	samples, rate, err := readWAV(path)
	if err != nil {
		t.Skipf("fixture unavailable: %v", err)
	}
	if rate != 16000 {
		t.Skipf("fixture rate %d != 16000", rate)
	}

	cfg := hostCfg()
	cfg.EnableVAD = true
	cfg.VadTier = VadFast
	cfg.VadAggressiveness = 1
	cfg.EnableAnomaly = false
	e, err := New(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer e.Close()
	if err := e.Start(); err != nil {
		t.Fatal(err)
	}

	// Single consumer in the test goroutine (no competing reader): non-blocking
	// drain of both channels, counting speech onsets.
	speechBegin := 0
	drainCount := func() {
		for {
			select {
			case ev := <-e.Events():
				if ev.Type == EventSpeechBegin {
					speechBegin++
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
			time.Sleep(20 * time.Millisecond) // pace so the ring stays drained
			drainCount()
		}
	}
	time.Sleep(300 * time.Millisecond) // let the worker finish the backlog
	drainCount()
	if err := e.Stop(); err != nil {
		t.Fatal(err)
	}
	drainCount()

	if speechBegin < 1 {
		t.Errorf("real voice produced %d SpeechBegin events, want >= 1", speechBegin)
	}
}

// drainFor reads (and discards) packets for d, keeping the channels flowing.
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

// readWAV reads a 16-bit PCM mono WAV (skips unknown chunks).
func readWAV(path string) (samples []int16, rate uint32, err error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, 0, err
	}
	if len(b) < 12 || string(b[0:4]) != "RIFF" || string(b[8:12]) != "WAVE" {
		return nil, 0, os.ErrInvalid
	}
	i := 12
	for i+8 <= len(b) {
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
		if sz%2 == 1 {
			sz++
		}
		i += 8 + sz
	}
	return nil, 0, os.ErrInvalid
}
