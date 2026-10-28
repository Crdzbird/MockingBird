import 'package:mockingbird_platform_interface/src/mockingbird_native_worker_stats.dart';

/// A mirror of `mockingbird_stats_t`.
final class MockingbirdNativeStats {
  /// Creates the record.
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
    required MockingbirdNativeWorkerStats worker,
    required int voiceProcessingActive,
    required int captureRestarts,
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
    required this.worker,
    required this.voiceProcessingActive,
    required this.captureRestarts,
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

  /// The worker-health fields.
  final MockingbirdNativeWorkerStats worker;

  /// Mirrors `mockingbird_stats_t.voice_processing_active`.
  final int voiceProcessingActive;

  /// Mirrors `mockingbird_stats_t.capture_restarts`.
  final int captureRestarts;
}
