part of 'mockingbird_event.dart';

/// A segment opened; packets will follow until the matching end event.
final class MockingbirdSegmentBeginEvent extends MockingbirdEvent {
  const MockingbirdSegmentBeginEvent._(
    int segmentId,
    int timestampUs,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// Confidence of the detection that opened the segment, 0..1.
  final double confidence;
}
