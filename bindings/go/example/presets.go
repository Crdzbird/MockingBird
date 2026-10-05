package main

import (
	"fmt"
	"strings"

	"github.com/Crdzbird/MockingBird/bindings/go"
)

func presetConfig(preset string, sensitivity float64, silero, yamnet string) mockingbird.Config {
	models := []mockingbird.Option{mockingbird.WithNeuralVADModel(silero), mockingbird.WithSoundClassModel(yamnet)}
	switch preset {
	case "voice":
		return mockingbird.Sensitive(0.6, append(models,
			mockingbird.WithDetectors(true, false, false), mockingbird.WithDenoise(true),
			mockingbird.WithEncoder(mockingbird.OpusEncoder()))...)
	case "everything":
		return mockingbird.Sensitive(0.6, append(models,
			mockingbird.WithDenoise(true), mockingbird.WithEncoder(mockingbird.OpusEncoder()))...)
	case "anomaly":
		c := mockingbird.DefaultConfig()
		c.VAD.Enabled = false
		c.Anomaly.RMSZ, c.Anomaly.FluxZ = 2, 2
		c.Encoder = mockingbird.OpusEncoder()
		return c
	case "denoised-voice":
		c := mockingbird.DefaultConfig()
		c.Denoise = true
		c.VAD.Tier, c.VAD.ModelPath = mockingbird.VadNeural, silero
		c.Anomaly.Enabled = false
		c.Encoder = mockingbird.OpusEncoder()
		return c
	case "music":
		c := mockingbird.DefaultConfig()
		c.VAD.Enabled, c.Anomaly.Enabled = false, false
		c.SoundClass = mockingbird.SoundClassConfig{Enabled: true, ModelPath: yamnet, Threshold: 0.3, IntervalMs: 1000}
		return c
	}
	if sensitivity >= 0 {
		return mockingbird.Sensitive(sensitivity, models...)
	}
	return mockingbird.DefaultConfig()
}

func describe(c mockingbird.Config, preset string) string {
	var on []string
	if c.VAD.Enabled {
		on = append(on, "VAD:"+c.VAD.Tier.String())
	}
	if c.Anomaly.Enabled {
		on = append(on, "anomaly")
	}
	if c.SoundClass.Enabled {
		on = append(on, "YAMNet")
	}
	if c.Denoise {
		on = append(on, "RNNoise")
	}
	if c.Performance.AdaptiveDegradation {
		on = append(on, "adaptive")
	}
	tag := ""
	if preset != "" {
		tag = "preset=" + preset + " · "
	}
	return fmt.Sprintf("%s%v · %d Hz · %v · [%s]", tag, c.Backend, c.SampleRateHz, c.Encoder.Codec, strings.Join(on, " "))
}
