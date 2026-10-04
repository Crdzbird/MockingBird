/// The segment, encoder, queue and performance fields of `auris_config_t`.
final class AurisNativeOutputConfig {
  /// Creates the output fields.
  const AurisNativeOutputConfig({
    required this.preRollMs,
    required this.postRollMs,
    required this.segmentMaxMs,
    required this.codec,
    required this.opusBitrateBps,
    required this.opusVbr,
    required this.opusComplexity,
    required this.pcmRingFrames,
    required this.packetQueueCapacity,
    required this.enablePcmTap,
    required this.pcmTapFrames,
    required this.silenceGateDbfs,
    required this.enableAdaptiveDegradation,
    required this.degradeBusyPpm,
    required this.workerBatchFrames,
  });

  /// Mirrors `pre_roll_ms`.
  final int preRollMs;

  /// Mirrors `post_roll_ms`.
  final int postRollMs;

  /// Mirrors `segment_max_ms`.
  final int segmentMaxMs;

  /// `auris_codec_t` as an integer.
  final int codec;

  /// Mirrors `opus_bitrate_bps`.
  final int opusBitrateBps;

  /// Mirrors `opus_vbr`.
  final bool opusVbr;

  /// Mirrors `opus_complexity`.
  final int opusComplexity;

  /// Mirrors `pcm_ring_frames`.
  final int pcmRingFrames;

  /// Mirrors `packet_queue_capacity`.
  final int packetQueueCapacity;

  /// Mirrors `enable_pcm_tap`.
  final bool enablePcmTap;

  /// Mirrors `pcm_tap_frames`.
  final int pcmTapFrames;

  /// Mirrors `silence_gate_dbfs`.
  final double silenceGateDbfs;

  /// Mirrors `enable_adaptive_degradation`.
  final bool enableAdaptiveDegradation;

  /// Mirrors `degrade_busy_ppm`.
  final int degradeBusyPpm;

  /// Mirrors `worker_batch_frames`.
  final int workerBatchFrames;
}
