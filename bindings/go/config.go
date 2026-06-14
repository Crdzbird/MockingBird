package auris

/*
#include <stdlib.h>
#include "auris/auris.h"
*/
import "C"

import (
	"math"
	"unsafe"
)

// Backend selects the capture source. Mirrors auris_backend_t.
type Backend int

const (
	BackendAuto      Backend = 0 // best available for the platform
	BackendMiniaudio Backend = 1 // portable default
	BackendOboe      Backend = 2 // Android low-latency
	BackendCoreAudio Backend = 3 // iOS + macOS
	BackendHostPCM   Backend = 4 // no internal capture; host pushes PCM
)

// VadTier selects the voice-activity detector. Both tiers are language-agnostic.
type VadTier int

const (
	VadFast   VadTier = 0 // libfvad (GMM), zero-dependency
	VadNeural VadTier = 1 // Silero via ONNX Runtime, build-flag gated
)

// Codec selects the output encoding. Mirrors auris_codec_t.
type Codec int

const (
	CodecPCMS16LE Codec = 0 // passthrough
	CodecOpus     Codec = 1 // live path
	CodecFLAC     Codec = 2 // archival (planned)
)

// Config configures an engine. Build one with DefaultConfig or Sensitive and
// adjust fields, then pass it to New. All fields are validated by New.
type Config struct {
	Backend      Backend
	SampleRateHz uint32 // 8000 / 16000 / 32000 / 48000
	Channels     uint16 // v1: 1 (mono)
	FrameMs      uint32 // 10 / 20 / 30

	// Preprocess.
	EnableRNNoise bool // noise suppression before every detector; needs an
	// integer 48 kHz ratio (8/16/48 kHz).

	// Voice activity detection (multilingual).
	EnableVAD          bool
	VadTier            VadTier
	VadAggressiveness  int     // libfvad 0..3 (higher = stricter). FAST only.
	VadNeuralThreshold float32 // Silero P(speech) 0..1. NEURAL only.
	VadHangoverMs      uint32  // trailing silence before SPEECH_END
	NeuralVADModelPath string  // path to silero_vad.onnx (NEURAL only)

	// Anomaly gate (non-speech acoustic events).
	EnableAnomaly       bool
	AnomalyRMSZ         float32 // z-score trigger; lower = more sensitive
	AnomalyFluxZ        float32
	AnomalyBackgroundMs uint32 // rolling background window

	// Segmentation.
	PreRollMs    uint32
	PostRollMs   uint32
	SegmentMaxMs uint32

	// Encoder.
	Codec          Codec
	OpusBitrateBps uint32
	OpusVBR        bool
	OpusComplexity int // 0..10

	// Queues.
	PcmRingFrames       uint32
	PacketQueueCapacity uint32

	EnableWaitHandle bool

	// Sound classification (YAMNet, all 521 AudioSet classes).
	EnableSoundClass     bool
	SoundClassModelPath  string  // path to yamnet_3s.onnx
	SoundClassThreshold  float32 // top-1 probability 0..1
	SoundClassIntervalMs uint32  // inference cadence
}

// DefaultConfig returns the library's documented defaults (mirrors
// auris_config_defaults): 16 kHz mono, 20 ms frames, fast VAD + anomaly on,
// PCM passthrough.
func DefaultConfig() Config {
	return Config{
		Backend:              BackendAuto,
		SampleRateHz:         16000,
		Channels:             1,
		FrameMs:              20,
		EnableRNNoise:        false,
		EnableVAD:            true,
		VadTier:              VadFast,
		VadAggressiveness:    2,
		VadNeuralThreshold:   0.5,
		VadHangoverMs:        300,
		EnableAnomaly:        true,
		AnomalyRMSZ:          3.0,
		AnomalyFluxZ:         3.0,
		AnomalyBackgroundMs:  2000,
		PreRollMs:            300,
		PostRollMs:           300,
		SegmentMaxMs:         60000,
		Codec:                CodecPCMS16LE,
		OpusBitrateBps:       32000,
		OpusVBR:              true,
		OpusComplexity:       5,
		PcmRingFrames:        1 << 14,
		PacketQueueCapacity:  256,
		EnableSoundClass:     false,
		SoundClassThreshold:  0.4,
		SoundClassIntervalMs: 500,
	}
}

// Option customizes Sensitive.
type Option func(*Config)

// WithBackend sets the capture backend (default BackendAuto).
func WithBackend(b Backend) Option { return func(c *Config) { c.Backend = b } }

// WithSampleRate sets the working sample rate (default 16000).
func WithSampleRate(hz uint32) Option { return func(c *Config) { c.SampleRateHz = hz } }

// WithVadTier overrides the VAD tier (Sensitive defaults to VadNeural).
func WithVadTier(t VadTier) Option { return func(c *Config) { c.VadTier = t } }

// WithRNNoise enables/disables RNNoise preprocessing.
func WithRNNoise(on bool) Option { return func(c *Config) { c.EnableRNNoise = on } }

// WithNeuralVADModel sets the Silero ONNX model path.
func WithNeuralVADModel(path string) Option {
	return func(c *Config) { c.NeuralVADModelPath = path }
}

// WithSoundClassModel sets the YAMNet ONNX model path.
func WithSoundClassModel(path string) Option {
	return func(c *Config) { c.SoundClassModelPath = path }
}

// WithDetectors toggles the three detectors at once.
func WithDetectors(vad, anomaly, soundClass bool) Option {
	return func(c *Config) {
		c.EnableVAD = vad
		c.EnableAnomaly = anomaly
		c.EnableSoundClass = soundClass
	}
}

// Sensitive builds a Config from a single 0..1 dial that drives every detector
// at once (mirrors AurisConfig.sensitive in the Dart binding): 0 is least
// sensitive (fewest detections / false positives), 1 is most sensitive. Voice
// detection stays multilingual regardless of the dial. By default it enables
// all three detectors with neural (Silero) VAD.
//
//	Silero P(speech):   0.80 .. 0.20
//	libfvad aggr.:      3    .. 0
//	anomaly RMS/flux z: 5.0  .. 1.5
//	YAMNet top-1:       0.70 .. 0.12
func Sensitive(sensitivity float64, opts ...Option) Config {
	s := math.Max(0, math.Min(1, sensitivity))
	lerp := func(a, b float64) float64 { return a + (b-a)*s }

	c := DefaultConfig()
	c.VadTier = VadNeural
	c.EnableVAD = true
	c.EnableAnomaly = true
	c.EnableSoundClass = true
	c.VadAggressiveness = clampInt(int(math.Round(lerp(3, 0))), 0, 3)
	c.VadNeuralThreshold = float32(lerp(0.80, 0.20))
	c.AnomalyRMSZ = float32(lerp(5.0, 1.5))
	c.AnomalyFluxZ = float32(lerp(5.0, 1.5))
	c.SoundClassThreshold = float32(lerp(0.70, 0.12))

	for _, o := range opts {
		o(&c)
	}
	return c
}

func clampInt(v, lo, hi int) int {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

func boolToC(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// toC marshals the config into the C struct. The returned func frees the
// C-allocated model-path strings; call it after auris_create returns (the
// engine deep-copies the strings it needs).
func (cfg Config) toC() (C.auris_config_t, func()) {
	var c C.auris_config_t
	C.auris_config_defaults(&c) // zeroes _reserved + sets sample_format

	c.backend = C.auris_backend_t(cfg.Backend)
	c.sample_rate_hz = C.uint32_t(cfg.SampleRateHz)
	c.channels = C.uint16_t(cfg.Channels)
	c.frame_ms = C.uint32_t(cfg.FrameMs)

	c.enable_rnnoise = boolToC(cfg.EnableRNNoise)

	c.enable_vad = boolToC(cfg.EnableVAD)
	c.vad_tier = C.auris_vad_tier_t(cfg.VadTier)
	c.vad_aggressiveness = C.int(cfg.VadAggressiveness)
	c.vad_neural_threshold = C.float(cfg.VadNeuralThreshold)
	c.vad_hangover_ms = C.uint32_t(cfg.VadHangoverMs)

	c.enable_anomaly = boolToC(cfg.EnableAnomaly)
	c.anomaly_rms_z = C.float(cfg.AnomalyRMSZ)
	c.anomaly_flux_z = C.float(cfg.AnomalyFluxZ)
	c.anomaly_background_ms = C.uint32_t(cfg.AnomalyBackgroundMs)

	c.pre_roll_ms = C.uint32_t(cfg.PreRollMs)
	c.post_roll_ms = C.uint32_t(cfg.PostRollMs)
	c.segment_max_ms = C.uint32_t(cfg.SegmentMaxMs)

	c.codec = C.auris_codec_t(cfg.Codec)
	c.opus_bitrate_bps = C.uint32_t(cfg.OpusBitrateBps)
	c.opus_vbr = boolToC(cfg.OpusVBR)
	c.opus_complexity = C.int(cfg.OpusComplexity)

	c.pcm_ring_frames = C.uint32_t(cfg.PcmRingFrames)
	c.packet_queue_capacity = C.uint32_t(cfg.PacketQueueCapacity)
	c.enable_wait_handle = boolToC(cfg.EnableWaitHandle)

	c.enable_sound_class = boolToC(cfg.EnableSoundClass)
	c.sound_class_threshold = C.float(cfg.SoundClassThreshold)
	c.sound_class_interval_ms = C.uint32_t(cfg.SoundClassIntervalMs)

	var frees []func()
	setStr := func(dst **C.char, s string) {
		if s == "" {
			*dst = nil
			return
		}
		cs := C.CString(s)
		*dst = cs
		frees = append(frees, func() { C.free(unsafe.Pointer(cs)) })
	}
	setStr(&c.neural_vad_model_path, cfg.NeuralVADModelPath)
	setStr(&c.sound_class_model_path, cfg.SoundClassModelPath)

	return c, func() {
		for _, f := range frees {
			f()
		}
	}
}
