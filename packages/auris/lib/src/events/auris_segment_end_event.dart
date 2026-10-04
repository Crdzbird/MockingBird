part of 'auris_event.dart';

/// A segment closed; no more packets carry its id.
final class AurisSegmentEndEvent extends AurisEvent {
  const AurisSegmentEndEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
