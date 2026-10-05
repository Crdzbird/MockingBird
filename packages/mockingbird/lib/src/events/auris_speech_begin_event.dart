part of 'auris_event.dart';

/// A voice started speaking, in any language.
final class AurisSpeechBeginEvent extends AurisEvent {
  const AurisSpeechBeginEvent._(int segmentId, int timestampUs, this.confidence)
    : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The detector's confidence, 0..1.
  final double confidence;
}
