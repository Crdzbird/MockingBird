package auris

import (
	"math"
	"testing"
)

func approx(a, b float32) bool { return math.Abs(float64(a-b)) < 1e-4 }

func TestSensitiveEndpoints(t *testing.T) {
	low, high := Sensitive(0), Sensitive(1)
	if !approx(low.VAD.NeuralThreshold, 0.80) || !approx(high.VAD.NeuralThreshold, 0.20) {
		t.Errorf("VAD threshold: low=%v high=%v", low.VAD.NeuralThreshold, high.VAD.NeuralThreshold)
	}
	if low.VAD.Aggressiveness != 3 || high.VAD.Aggressiveness != 0 {
		t.Errorf("aggressiveness: low=%d high=%d", low.VAD.Aggressiveness, high.VAD.Aggressiveness)
	}
	if !approx(low.Anomaly.RMSZ, 5.0) || !approx(high.Anomaly.RMSZ, 1.5) {
		t.Errorf("anomaly z: low=%v high=%v", low.Anomaly.RMSZ, high.Anomaly.RMSZ)
	}
	if !approx(low.SoundClass.Threshold, 0.70) || !approx(high.SoundClass.Threshold, 0.12) {
		t.Errorf("YAMNet threshold: low=%v high=%v", low.SoundClass.Threshold, high.SoundClass.Threshold)
	}
}

func TestSensitivityClampsAndDefaults(t *testing.T) {
	if NewSensitivity(5) != 1 || NewSensitivity(-5) != 0 {
		t.Error("sensitivity not clamped to 0..1")
	}
	c := Sensitive(0.5)
	if c.VAD.Tier != VadNeural || !c.VAD.Enabled || !c.Anomaly.Enabled || !c.SoundClass.Enabled {
		t.Error("Sensitive should enable every detector with neural VAD")
	}
}

func TestSensitiveOptions(t *testing.T) {
	c := Sensitive(0.5,
		WithBackend(BackendHostPCM),
		WithDenoise(true),
		WithVadTier(VadFast),
		WithEncoder(OpusEncoder()),
		WithPerformance(MobilePerformance()),
		WithPCMTap(0),
	)
	if c.Backend != BackendHostPCM || !c.Denoise || c.VAD.Tier != VadFast {
		t.Error("basic options not applied")
	}
	if c.Encoder.Codec != CodecOpus || !c.Performance.AdaptiveDegradation || !c.Queues.PCMTap {
		t.Error("group options not applied")
	}
}

func TestDefaultsMatchTheNativeEngine(t *testing.T) {
	c := DefaultConfig()
	if c.Performance.SilenceGateDbfs != -60 || c.Performance.AdaptiveDegradation ||
		c.Performance.DegradeBusyPpm != 700000 || c.SoundClass.IntervalMs != 1000 {
		t.Errorf("unexpected defaults: %+v", c)
	}
	if EventDegradation.String() != "DEGRADATION" || CodecOpus.String() != "opus" {
		t.Error("String() names")
	}
}
