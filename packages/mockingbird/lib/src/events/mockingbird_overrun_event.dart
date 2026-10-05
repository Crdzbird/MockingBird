part of 'mockingbird_event.dart';

/// The host fell behind and the packet queue dropped its oldest packet.
final class MockingbirdOverrunEvent extends MockingbirdEvent {
  const MockingbirdOverrunEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
