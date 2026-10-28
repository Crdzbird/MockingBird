import 'package:mockingbird/src/mockingbird_capture_health.dart';
import 'package:mockingbird/src/mockingbird_worker_health.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Engine counters at one moment; never carries audio.
final class MockingbirdStats {
  /// Creates a snapshot.
  const factory MockingbirdStats({
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
    required MockingbirdWorkerHealth worker,
    required MockingbirdCaptureHealth capture,
  }) = MockingbirdStats._;

  const MockingbirdStats._({
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
    required this.capture,
  });

  /// Reads a native `mockingbird_stats_t`.
  factory MockingbirdStats.fromNative(MockingbirdNativeStats s) =>
      MockingbirdStats(
        framesCaptured: s.framesCaptured,
        framesDroppedCapture: s.framesDroppedCapture,
        packetsEmitted: s.packetsEmitted,
        packetsDropped: s.packetsDropped,
        segmentsOpened: s.segmentsOpened,
        segmentsClosed: s.segmentsClosed,
        pcmRingFillFrames: s.pcmRingFillFrames,
        packetQueueFill: s.packetQueueFill,
        inputRmsDbfs: s.inputRmsDbfs,
        inputPeakDbfs: s.inputPeakDbfs,
        tapSamplesDropped: s.tapSamplesDropped,
        worker: MockingbirdWorkerHealth.fromNative(s.worker),
        capture: MockingbirdCaptureHealth.fromNative(s),
      );

  /// Samples accepted from the capture source.
  final int framesCaptured;

  /// Samples lost because the capture ring was full.
  final int framesDroppedCapture;

  /// Packets handed to the host queue.
  final int packetsEmitted;

  /// Packets dropped because the host queue was full.
  final int packetsDropped;

  /// Segments opened so far.
  final int segmentsOpened;

  /// Segments closed so far.
  final int segmentsClosed;

  /// Samples waiting in the capture ring.
  final int pcmRingFillFrames;

  /// Packets waiting for the host.
  final int packetQueueFill;

  /// Input RMS of the latest frame in dBFS, measured before denoising.
  final double inputRmsDbfs;

  /// Input peak of the latest frame in dBFS.
  final double inputPeakDbfs;

  /// Samples the PCM tap dropped because the host did not drain it.
  final int tapSamplesDropped;

  /// Worker-thread health and work shedding.
  final MockingbirdWorkerHealth worker;

  /// Voice processing in effect and capture restarts.
  final MockingbirdCaptureHealth capture;
}
