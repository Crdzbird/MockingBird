/// A flat mirror of `mockingbird_stats_t`.
final class MockingbirdNativeStats {
  /// Creates a stats record.
  const factory MockingbirdNativeStats({
    required int framesCaptured,
    required int framesDroppedCapture,
    required int packetsEmitted,
    required int packetsDropped,
    required int segmentsOpened,
    required int segmentsClosed,
    required int pcmRingFillFrames,
    required int packetQueueFill,
    required double inputRmsDbfs,
    required double inputPeakDbfs,
    required int tapSamplesDropped,
    required int workerBusyPpm,
    required int workerMaxFrameUs,
    required int workerWakeups,
    required int soundClassInferences,
    required int soundClassInferencesSkipped,
    required int preprocFramesSkipped,
    required int degradationLevel,
  }) = MockingbirdNativeStats._;

  const MockingbirdNativeStats._({
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

  /// Mirrors `mockingbird_stats_t.frames_captured`.
  final int framesCaptured;

  /// Mirrors `mockingbird_stats_t.frames_dropped_capture`.
  final int framesDroppedCapture;

  /// Mirrors `mockingbird_stats_t.packets_emitted`.
  final int packetsEmitted;

  /// Mirrors `mockingbird_stats_t.packets_dropped`.
  final int packetsDropped;

  /// Mirrors `mockingbird_stats_t.segments_opened`.
  final int segmentsOpened;

  /// Mirrors `mockingbird_stats_t.segments_closed`.
  final int segmentsClosed;

  /// Mirrors `mockingbird_stats_t.pcm_ring_fill_frames`.
  final int pcmRingFillFrames;

  /// Mirrors `mockingbird_stats_t.packet_queue_fill`.
  final int packetQueueFill;

  /// Mirrors `mockingbird_stats_t.input_rms_dbfs`.
  final double inputRmsDbfs;

  /// Mirrors `mockingbird_stats_t.input_peak_dbfs`.
  final double inputPeakDbfs;

  /// Mirrors `mockingbird_stats_t.tap_samples_dropped`.
  final int tapSamplesDropped;

  /// Mirrors `mockingbird_stats_t.worker_busy_ppm`.
  final int workerBusyPpm;

  /// Mirrors `mockingbird_stats_t.worker_max_frame_us`.
  final int workerMaxFrameUs;

  /// Mirrors `mockingbird_stats_t.worker_wakeups`.
  final int workerWakeups;

  /// Mirrors `mockingbird_stats_t.sound_class_inferences`.
  final int soundClassInferences;

  /// Mirrors `mockingbird_stats_t.sound_class_inferences_skipped`.
  final int soundClassInferencesSkipped;

  /// Mirrors `mockingbird_stats_t.preproc_frames_skipped`.
  final int preprocFramesSkipped;

  /// Mirrors `mockingbird_stats_t.degradation_level`.
  final int degradationLevel;
}
