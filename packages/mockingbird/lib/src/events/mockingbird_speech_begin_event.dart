part of 'mockingbird_event.dart';

/// A voice started speaking, in any language.
///
/// [timestampUs] is where the speech began. The neural tier confirms speech
/// a window or two after it starts and reports the start, never earlier than
/// the segment's first packet.
final class MockingbirdSpeechBeginEvent extends MockingbirdEvent {
  const MockingbirdSpeechBeginEvent._(
    int segmentId,
    int timestampUs,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The detector's confidence, 0..1.
  final double confidence;
}
