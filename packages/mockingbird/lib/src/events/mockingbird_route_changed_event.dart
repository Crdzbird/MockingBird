part of 'mockingbird_event.dart';

/// The input device changed, a headset plugged or unplugged or the default
/// switched, and capture continues on the new route.
final class MockingbirdRouteChangedEvent extends MockingbirdEvent {
  const MockingbirdRouteChangedEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
