/// The worker-health fields of `mockingbird_stats_t`.
final class MockingbirdNativeWorkerStats {
  /// Creates the record.
  const factory MockingbirdNativeWorkerStats({
    required int busyPpm,
    required int maxFrameUs,
    required int wakeups,
    required int soundClassInferences,
    required int soundClassInferencesSkipped,
    required int preprocFramesSkipped,
    required int degradationLevel,
  }) = MockingbirdNativeWorkerStats._;

  const MockingbirdNativeWorkerStats._({
    required this.busyPpm,
    required this.maxFrameUs,
    required this.wakeups,
    required this.soundClassInferences,
    required this.soundClassInferencesSkipped,
    required this.preprocFramesSkipped,
    required this.degradationLevel,
  });

  /// Mirrors `mockingbird_stats_t.worker_busy_ppm`.
  final int busyPpm;

  /// Mirrors `mockingbird_stats_t.worker_max_frame_us`.
  final int maxFrameUs;

  /// Mirrors `mockingbird_stats_t.worker_wakeups`.
  final int wakeups;

  /// Mirrors `mockingbird_stats_t.sound_class_inferences`.
  final int soundClassInferences;

  /// Mirrors `mockingbird_stats_t.sound_class_inferences_skipped`.
  final int soundClassInferencesSkipped;

  /// Mirrors `mockingbird_stats_t.preproc_frames_skipped`.
  final int preprocFramesSkipped;

  /// Mirrors `mockingbird_stats_t.degradation_level`.
  final int degradationLevel;
}
