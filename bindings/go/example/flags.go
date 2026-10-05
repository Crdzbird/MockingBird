package main

import (
	"fmt"
	"os"

	"github.com/Crdzbird/MockingBird/bindings/go"
)

type flags struct {
	preset, file, vad, anomaly, sound, denoise, codec string
	silero, yamnet                                    string
	sensitivity, gate                                 float64
	seconds                                           int
}

func (f flags) config() mockingbird.Config {
	silero := firstNonEmpty(f.silero, locateFixture("silero_vad.onnx"))
	yamnet := firstNonEmpty(f.yamnet, locateFixture("yamnet_3s.onnx"))
	live := f.file == ""
	cfg := presetConfig(f.preset, f.sensitivity, silero, yamnet)
	if !live {
		cfg.Backend = mockingbird.BackendHostPCM
		cfg.Queues.PCMRingFrames = 1 << 17
		cfg.Queues.PacketQueueCapacity = 4096
	} else {
		cfg.Performance = mockingbird.MobilePerformance()
	}
	switch f.vad {
	case "off":
		cfg.VAD.Enabled = false
	case "fast":
		cfg.VAD.Enabled, cfg.VAD.Tier = true, mockingbird.VadFast
	case "neural":
		cfg.VAD.Enabled, cfg.VAD.Tier, cfg.VAD.ModelPath = true, mockingbird.VadNeural, silero
	}
	setBool(&cfg.Anomaly.Enabled, f.anomaly)
	if setBool(&cfg.SoundClass.Enabled, f.sound) {
		cfg.SoundClass.ModelPath = yamnet
	}
	setBool(&cfg.Denoise, f.denoise)
	switch f.codec {
	case "pcm":
		cfg.Encoder.Codec = mockingbird.CodecPCMS16LE
	case "opus":
		cfg.Encoder.Codec = mockingbird.CodecOpus
	}
	if f.gate != 0 {
		cfg.Performance.SilenceGateDbfs = float32(f.gate)
	}
	return sanitize(cfg, silero, yamnet)
}

func sanitize(cfg mockingbird.Config, silero, yamnet string) mockingbird.Config {
	if cfg.VAD.Enabled && cfg.VAD.Tier == mockingbird.VadNeural && silero == "" {
		fmt.Fprintln(os.Stderr, "note: no silero_vad.onnx found, using fast VAD")
		cfg.VAD.Tier = mockingbird.VadFast
	}
	if cfg.SoundClass.Enabled && yamnet == "" {
		fmt.Fprintln(os.Stderr, "note: no yamnet_3s.onnx found, sound classification off")
		cfg.SoundClass.Enabled = false
	}
	if cfg.Denoise && 48000%cfg.SampleRateHz != 0 {
		fmt.Fprintln(os.Stderr, "note: RNNoise needs an 8, 16 or 48 kHz rate, denoise off")
		cfg.Denoise = false
	}
	return cfg
}

func setBool(dst *bool, v string) bool {
	switch v {
	case "on", "true", "1":
		*dst = true
	case "off", "false", "0":
		*dst = false
	default:
		return false
	}
	return true
}

func firstNonEmpty(a, b string) string {
	if a != "" {
		return a
	}
	return b
}
