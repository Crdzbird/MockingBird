part of 'mockingbird_event.dart';

/// A voice started speaking, in any language.
final class MockingbirdSpeechBeginEvent extends MockingbirdEvent {
  const MockingbirdSpeechBeginEvent._(
    int segmentId,
    int timestampUs,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The detector's confidence, 0..1.
  final double confidence;
}
