part of 'auris_event.dart';

/// An event type this version of the Dart package does not know yet.
final class AurisUnknownEvent extends AurisEvent {
  const AurisUnknownEvent._(int segmentId, int timestampUs, this.nativeType)
    : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The native `auris_event_type_t` value.
  final int nativeType;
}
