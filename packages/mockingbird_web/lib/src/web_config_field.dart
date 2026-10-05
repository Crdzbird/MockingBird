/// Config field ids, in the order of `src/web/web_fields.hpp`.
enum WebConfigField {
  /// `backend`.
  backend,

  /// `sample_rate_hz`.
  sampleRateHz,

  /// `frame_ms`.
  frameMs,

  /// `enable_rnnoise`.
  enableRnnoise,

  /// `enable_vad`.
  enableVad,

  /// `vad_tier`.
  vadTier,

  /// `vad_aggressiveness`.
  vadAggressiveness,

  /// `vad_neural_threshold`.
  vadNeuralThreshold,

  /// `vad_hangover_ms`.
  vadHangoverMs,

  /// `enable_anomaly`.
  enableAnomaly,

  /// `anomaly_rms_z`.
  anomalyRmsZ,

  /// `anomaly_flux_z`.
  anomalyFluxZ,

  /// `anomaly_background_ms`.
  anomalyBackgroundMs,

  /// `enable_sound_class`.
  enableSoundClass,

  /// `sound_class_threshold`.
  soundClassThreshold,

  /// `sound_class_interval_ms`.
  soundClassIntervalMs,

  /// `pre_roll_ms`.
  preRollMs,

  /// `post_roll_ms`.
  postRollMs,

  /// `segment_max_ms`.
  segmentMaxMs,

  /// `codec`.
  codec,

  /// `opus_bitrate_bps`.
  opusBitrateBps,

  /// `opus_vbr`.
  opusVbr,

  /// `opus_complexity`.
  opusComplexity,

  /// `pcm_ring_frames`.
  pcmRingFrames,

  /// `packet_queue_capacity`.
  packetQueueCapacity,

  /// `enable_pcm_tap`.
  enablePcmTap,

  /// `pcm_tap_frames`.
  pcmTapFrames,

  /// `silence_gate_dbfs`.
  silenceGateDbfs,

  /// `enable_adaptive_degradation`.
  enableAdaptiveDegradation,

  /// `degrade_busy_ppm`.
  degradeBusyPpm,

  /// `worker_batch_frames`.
  workerBatchFrames,
}
