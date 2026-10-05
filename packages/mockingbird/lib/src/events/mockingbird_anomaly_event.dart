part of 'mockingbird_event.dart';

/// A sudden or unusual sound tripped the anomaly gate.
final class MockingbirdAnomalyEvent extends MockingbirdEvent {
  const MockingbirdAnomalyEvent._(
    int segmentId,
    int timestampUs,
    this.reasonMask,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// Raw reason bits; prefer [reasons].
  final int reasonMask;

  /// How strongly the gate fired, 0..1.
  final double confidence;

  /// Which statistics tripped the gate.
  Set<MockingbirdAnomalyReason> get reasons =>
      MockingbirdAnomalyReason.fromMask(reasonMask);
}
