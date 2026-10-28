part of 'mockingbird_event.dart';

/// Capture is running again after an interruption.
final class MockingbirdCaptureResumedEvent extends MockingbirdEvent {
  const MockingbirdCaptureResumedEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
