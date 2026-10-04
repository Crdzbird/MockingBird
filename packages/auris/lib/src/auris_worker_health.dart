import 'package:auris_platform_interface/auris_platform_interface.dart';

/// How hard the native worker thread is working and what it is shedding.
final class AurisWorkerHealth {
  /// Creates a snapshot.
  const AurisWorkerHealth({
    required this.busyPpm,
    required this.maxFrameUs,
    required this.wakeups,
    required this.soundClassInferences,
    required this.soundClassInferencesSkipped,
    required this.preprocFramesSkipped,
    required this.degradationLevel,
  });

  /// Reads the worker fields of a native `auris_stats_t`.
  factory AurisWorkerHealth.fromNative(auris_stats_t s) => AurisWorkerHealth(
    busyPpm: s.worker_busy_ppm,
    maxFrameUs: s.worker_max_frame_us,
    wakeups: s.worker_wakeups,
    soundClassInferences: s.sound_class_inferences,
    soundClassInferencesSkipped: s.sound_class_inferences_skipped,
    preprocFramesSkipped: s.preproc_frames_skipped,
    degradationLevel: s.degradation_level,
  );

  /// Share of wall time spent processing over the last ~1 s, in parts per
  /// million (1 000 000 = saturated).
  final int busyPpm;

  /// Longest single-frame processing time since start, microseconds.
  final int maxFrameUs;

  /// Thread parks/unparks; on phones a better battery proxy than CPU share.
  final int wakeups;

  /// YAMNet inferences that ran.
  final int soundClassInferences;

  /// YAMNet inferences shed by the silence gate or degradation.
  final int soundClassInferencesSkipped;

  /// Frames RNNoise skipped because they were below the silence gate.
  final int preprocFramesSkipped;

  /// Adaptive-degradation level: 0 nominal .. 3 classifier suspended.
  final int degradationLevel;

  /// [busyPpm] as a 0..1 fraction.
  double get busyFraction => busyPpm / 1e6;
}
