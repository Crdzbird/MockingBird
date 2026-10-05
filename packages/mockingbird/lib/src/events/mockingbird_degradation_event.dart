part of 'mockingbird_event.dart';

/// Adaptive degradation changed level.
final class MockingbirdDegradationEvent extends MockingbirdEvent {
  const MockingbirdDegradationEvent._(
    int segmentId,
    int timestampUs,
    this.level,
    this.busyFraction,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The new level: 0 nominal .. 3 classifier suspended.
  final int level;

  /// The worker's busy share that triggered the change, 0..1.
  final double busyFraction;
}
