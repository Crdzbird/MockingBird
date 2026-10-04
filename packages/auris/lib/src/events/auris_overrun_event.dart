part of 'auris_event.dart';

/// The host fell behind and the packet queue dropped its oldest packet.
final class AurisOverrunEvent extends AurisEvent {
  const AurisOverrunEvent._(int segmentId, int timestampUs)
    : super(segmentId: segmentId, timestampUs: timestampUs);
}
