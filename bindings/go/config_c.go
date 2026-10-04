package auris

/*
#include <stdlib.h>
#include "auris/auris.h"
*/
import "C"

import "unsafe"

func boolToC(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

func (cfg Config) toC() (C.auris_config_t, func()) {
	var c C.auris_config_t
	C.auris_config_defaults(&c)
	c.backend = C.auris_backend_t(cfg.Backend)
	c.sample_rate_hz = C.uint32_t(cfg.SampleRateHz)
	c.channels = 1
	c.frame_ms = C.uint32_t(cfg.FrameMs)
	c.enable_rnnoise = boolToC(cfg.Denoise)
	c.enable_vad = boolToC(cfg.VAD.Enabled)
	c.vad_tier = C.auris_vad_tier_t(cfg.VAD.Tier)
	c.vad_aggressiveness = C.int(cfg.VAD.Aggressiveness)
	c.vad_neural_threshold = C.float(cfg.VAD.NeuralThreshold)
	c.vad_hangover_ms = C.uint32_t(cfg.VAD.HangoverMs)
	c.enable_anomaly = boolToC(cfg.Anomaly.Enabled)
	c.anomaly_rms_z = C.float(cfg.Anomaly.RMSZ)
	c.anomaly_flux_z = C.float(cfg.Anomaly.FluxZ)
	c.anomaly_background_ms = C.uint32_t(cfg.Anomaly.BackgroundMs)
	c.enable_sound_class = boolToC(cfg.SoundClass.Enabled)
	c.sound_class_threshold = C.float(cfg.SoundClass.Threshold)
	c.sound_class_interval_ms = C.uint32_t(cfg.SoundClass.IntervalMs)
	c.pre_roll_ms = C.uint32_t(cfg.Segments.PreRollMs)
	c.post_roll_ms = C.uint32_t(cfg.Segments.PostRollMs)
	c.segment_max_ms = C.uint32_t(cfg.Segments.MaxMs)
	c.codec = C.auris_codec_t(cfg.Encoder.Codec)
	c.opus_bitrate_bps = C.uint32_t(cfg.Encoder.OpusBitrateBps)
	c.opus_vbr = boolToC(cfg.Encoder.OpusVBR)
	c.opus_complexity = C.int(cfg.Encoder.OpusComplexity)
	c.pcm_ring_frames = C.uint32_t(cfg.Queues.PCMRingFrames)
	c.packet_queue_capacity = C.uint32_t(cfg.Queues.PacketQueueCapacity)
	c.enable_pcm_tap = boolToC(cfg.Queues.PCMTap)
	c.pcm_tap_frames = C.uint32_t(cfg.Queues.PCMTapFrames)
	c.silence_gate_dbfs = C.float(cfg.Performance.SilenceGateDbfs)
	c.enable_adaptive_degradation = boolToC(cfg.Performance.AdaptiveDegradation)
	c.degrade_busy_ppm = C.uint32_t(cfg.Performance.DegradeBusyPpm)
	c.worker_batch_frames = C.uint32_t(cfg.Performance.WorkerBatchFrames)

	var owned []unsafe.Pointer
	cString := func(s string) *C.char {
		if s == "" {
			return nil
		}
		p := C.CString(s)
		owned = append(owned, unsafe.Pointer(p))
		return p
	}
	c.neural_vad_model_path = cString(cfg.VAD.ModelPath)
	c.sound_class_model_path = cString(cfg.SoundClass.ModelPath)
	return c, func() {
		for _, p := range owned {
			C.free(p)
		}
	}
}
