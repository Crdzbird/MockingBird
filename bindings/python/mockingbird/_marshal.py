"""Marshalling a :class:`Config` into the native struct."""

from __future__ import annotations

from mockingbird._config_struct import ConfigStruct
from mockingbird.config import Config


def to_struct(cfg: Config, defaults: ConfigStruct) -> ConfigStruct:
    """Fill ``defaults`` (already initialised by the library) from ``cfg``.
    The returned struct keeps its path buffers alive."""
    c = defaults
    c.backend = int(cfg.backend)
    c.sample_rate_hz = cfg.sample_rate_hz
    c.channels = 1
    c.frame_ms = cfg.frame_ms
    c.enable_rnnoise = int(cfg.denoise)
    c.voice_processing = int(cfg.voice_processing)
    c.enable_vad = int(cfg.vad.enabled)
    c.vad_tier = int(cfg.vad.tier)
    c.vad_aggressiveness = cfg.vad.aggressiveness
    c.vad_neural_threshold = cfg.vad.neural_threshold
    c.vad_hangover_ms = cfg.vad.hangover_ms
    c.enable_anomaly = int(cfg.anomaly.enabled)
    c.anomaly_rms_z = cfg.anomaly.rms_z
    c.anomaly_flux_z = cfg.anomaly.flux_z
    c.anomaly_background_ms = cfg.anomaly.background_ms
    c.enable_sound_class = int(cfg.sound_class.enabled)
    c.sound_class_threshold = cfg.sound_class.threshold
    c.sound_class_interval_ms = cfg.sound_class.interval_ms
    c.pre_roll_ms = cfg.segments.pre_roll_ms
    c.post_roll_ms = cfg.segments.post_roll_ms
    c.segment_max_ms = cfg.segments.max_ms
    c.codec = int(cfg.encoder.codec)
    c.opus_bitrate_bps = cfg.encoder.opus_bitrate_bps
    c.opus_vbr = int(cfg.encoder.opus_vbr)
    c.opus_complexity = cfg.encoder.opus_complexity
    c.pcm_ring_frames = cfg.queues.pcm_ring_frames
    c.packet_queue_capacity = cfg.queues.packet_queue_capacity
    c.enable_pcm_tap = int(cfg.queues.pcm_tap)
    c.pcm_tap_frames = cfg.queues.pcm_tap_frames
    c.silence_gate_dbfs = cfg.performance.silence_gate_dbfs
    c.enable_adaptive_degradation = int(cfg.performance.adaptive_degradation)
    c.degrade_busy_ppm = cfg.performance.degrade_busy_ppm
    c.worker_batch_frames = cfg.performance.worker_batch_frames
    c.keepalive = [p.encode() for p in (cfg.vad.model_path, cfg.sound_class.model_path) if p]
    c.neural_vad_model_path = cfg.vad.model_path.encode() if cfg.vad.model_path else None
    c.sound_class_model_path = (
        cfg.sound_class.model_path.encode() if cfg.sound_class.model_path else None
    )
    return c
