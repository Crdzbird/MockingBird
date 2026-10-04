package main

import (
	"fmt"
	"strings"

	"github.com/auris-audio/auris"
)

func presetConfig(preset string, sensitivity float64, silero, yamnet string) auris.Config {
	models := []auris.Option{auris.WithNeuralVADModel(silero), auris.WithSoundClassModel(yamnet)}
	switch preset {
	case "voice":
		return auris.Sensitive(0.6, append(models,
			auris.WithDetectors(true, false, false), auris.WithDenoise(true),
			auris.WithEncoder(auris.OpusEncoder()))...)
	case "everything":
		return auris.Sensitive(0.6, append(models,
			auris.WithDenoise(true), auris.WithEncoder(auris.OpusEncoder()))...)
	case "anomaly":
		c := auris.DefaultConfig()
		c.VAD.Enabled = false
		c.Anomaly.RMSZ, c.Anomaly.FluxZ = 2, 2
		c.Encoder = auris.OpusEncoder()
		return c
	case "denoised-voice":
		c := auris.DefaultConfig()
		c.Denoise = true
		c.VAD.Tier, c.VAD.ModelPath = auris.VadNeural, silero
		c.Anomaly.Enabled = false
		c.Encoder = auris.OpusEncoder()
		return c
	case "music":
		c := auris.DefaultConfig()
		c.VAD.Enabled, c.Anomaly.Enabled = false, false
		c.SoundClass = auris.SoundClassConfig{Enabled: true, ModelPath: yamnet, Threshold: 0.3, IntervalMs: 1000}
		return c
	}
	if sensitivity >= 0 {
		return auris.Sensitive(sensitivity, models...)
	}
	return auris.DefaultConfig()
}

func describe(c auris.Config, preset string) string {
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
