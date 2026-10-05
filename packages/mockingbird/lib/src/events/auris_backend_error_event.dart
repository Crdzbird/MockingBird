part of 'auris_event.dart';

/// The capture backend or a detector failed; capture may have stopped.
final class AurisBackendErrorEvent extends AurisEvent {
  AurisBackendErrorEvent._(int segmentId, int timestampUs, int nativeStatus)
    : status = AurisStatus.fromNative(nativeStatus),
      super(segmentId: segmentId, timestampUs: timestampUs);

  /// What went wrong.
  final AurisStatus status;
}
