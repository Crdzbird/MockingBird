import 'package:auris/src/auris_worker_health.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';

/// Engine counters at one moment; never carries audio.
final class AurisStats {
  /// Creates a snapshot.
  const AurisStats({
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
  });

  /// Reads a native `auris_stats_t`.
  factory AurisStats.fromNative(auris_stats_t s) => AurisStats(
    framesCaptured: s.frames_captured,
    framesDroppedCapture: s.frames_dropped_capture,
    packetsEmitted: s.packets_emitted,
    packetsDropped: s.packets_dropped,
    segmentsOpened: s.segments_opened,
    segmentsClosed: s.segments_closed,
    pcmRingFillFrames: s.pcm_ring_fill_frames,
    packetQueueFill: s.packet_queue_fill,
    inputRmsDbfs: s.input_rms_dbfs,
    inputPeakDbfs: s.input_peak_dbfs,
    tapSamplesDropped: s.tap_samples_dropped,
    worker: AurisWorkerHealth.fromNative(s),
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
  final AurisWorkerHealth worker;
}
