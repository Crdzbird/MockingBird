/// A flat mirror of `auris_stats_t`.
final class AurisNativeStats {
  /// Creates a stats record.
  const AurisNativeStats({
    required this.framesCaptured,
    required this.framesDroppedCapture,
    required this.packetsEmitted,
    required this.packetsDropped,
    required this.segmentsOpened,
    required this.segmentsClosed,
    required this.pcmRingFillFrames,
    required this.packetQueueFill,
    required this.inputRmsDbfs,
    required this.inputPeakDbfs,
    required this.tapSamplesDropped,
    required this.workerBusyPpm,
    required this.workerMaxFrameUs,
    required this.workerWakeups,
    required this.soundClassInferences,
    required this.soundClassInferencesSkipped,
    required this.preprocFramesSkipped,
    required this.degradationLevel,
  });

  /// Mirrors `auris_stats_t.frames_captured`.
  final int framesCaptured;

  /// Mirrors `auris_stats_t.frames_dropped_capture`.
  final int framesDroppedCapture;

  /// Mirrors `auris_stats_t.packets_emitted`.
  final int packetsEmitted;

  /// Mirrors `auris_stats_t.packets_dropped`.
  final int packetsDropped;

  /// Mirrors `auris_stats_t.segments_opened`.
  final int segmentsOpened;

  /// Mirrors `auris_stats_t.segments_closed`.
  final int segmentsClosed;

  /// Mirrors `auris_stats_t.pcm_ring_fill_frames`.
  final int pcmRingFillFrames;

  /// Mirrors `auris_stats_t.packet_queue_fill`.
  final int packetQueueFill;

  /// Mirrors `auris_stats_t.input_rms_dbfs`.
  final double inputRmsDbfs;

  /// Mirrors `auris_stats_t.input_peak_dbfs`.
  final double inputPeakDbfs;

  /// Mirrors `auris_stats_t.tap_samples_dropped`.
  final int tapSamplesDropped;

  /// Mirrors `auris_stats_t.worker_busy_ppm`.
  final int workerBusyPpm;

  /// Mirrors `auris_stats_t.worker_max_frame_us`.
  final int workerMaxFrameUs;

  /// Mirrors `auris_stats_t.worker_wakeups`.
  final int workerWakeups;

  /// Mirrors `auris_stats_t.sound_class_inferences`.
  final int soundClassInferences;

  /// Mirrors `auris_stats_t.sound_class_inferences_skipped`.
  final int soundClassInferencesSkipped;

  /// Mirrors `auris_stats_t.preproc_frames_skipped`.
  final int preprocFramesSkipped;

  /// Mirrors `auris_stats_t.degradation_level`.
  final int degradationLevel;
}
