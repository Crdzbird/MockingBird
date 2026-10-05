import 'package:mockingbird/src/config/mockingbird_config.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Flattens the segment, encoder, queue and performance groups.
final class NativeOutputMapper {
  /// Creates the mapper.
  const NativeOutputMapper();

  /// The output mirror of [c].
  MockingbirdNativeOutputConfig map(MockingbirdConfig c) =>
      MockingbirdNativeOutputConfig(
        preRollMs: c.segments.preRollMs,
        postRollMs: c.segments.postRollMs,
        segmentMaxMs: c.segments.maxMs,
        codec: c.encoder.codec.nativeValue,
        opusBitrateBps: c.encoder.opusBitrateBps,
        opusVbr: c.encoder.opusVbr,
        opusComplexity: c.encoder.opusComplexity,
        pcmRingFrames: c.queues.pcmRingFrames,
        packetQueueCapacity: c.queues.packetQueueCapacity,
        enablePcmTap: c.queues.pcmTap,
        pcmTapFrames: c.queues.pcmTapFrames,
        silenceGateDbfs: c.performance.silenceGateDbfs,
        enableAdaptiveDegradation: c.performance.adaptiveDegradation,
        degradeBusyPpm: c.performance.degradeBusyPpm,
        workerBatchFrames: c.performance.workerBatchFrames,
      );
}
