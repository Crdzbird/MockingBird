part of 'mockingbird_event.dart';

/// The voice stopped; the hangover has elapsed.
final class MockingbirdSpeechEndEvent extends MockingbirdEvent {
  const MockingbirdSpeechEndEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
