part of 'auris_event.dart';

/// The voice stopped; the hangover has elapsed.
final class AurisSpeechEndEvent extends AurisEvent {
  const AurisSpeechEndEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
