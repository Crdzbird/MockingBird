part of 'mockingbird_event.dart';

/// A segment closed; no more packets carry its id.
final class MockingbirdSegmentEndEvent extends MockingbirdEvent {
  const MockingbirdSegmentEndEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
