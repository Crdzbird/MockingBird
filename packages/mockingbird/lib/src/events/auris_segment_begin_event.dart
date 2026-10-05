part of 'auris_event.dart';

/// A segment opened; packets will follow until the matching end event.
final class AurisSegmentBeginEvent extends AurisEvent {
  const AurisSegmentBeginEvent._(
    int segmentId,
    int timestampUs,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// Confidence of the detection that opened the segment, 0..1.
  final double confidence;
}
