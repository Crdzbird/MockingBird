part of 'mockingbird_event.dart';

/// The operating system took the microphone away, for a call or another app's
/// recording; no audio arrives until [MockingbirdCaptureResumedEvent].
final class MockingbirdCaptureInterruptedEvent extends MockingbirdEvent {
  const MockingbirdCaptureInterruptedEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
