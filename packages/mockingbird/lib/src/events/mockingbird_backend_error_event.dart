part of 'mockingbird_event.dart';

/// The capture backend or a detector failed; capture may have stopped.
final class MockingbirdBackendErrorEvent extends MockingbirdEvent {
  MockingbirdBackendErrorEvent._(
    int segmentId,
    int timestampUs,
    int nativeStatus,
  ) : status = MockingbirdStatus.fromNative(nativeStatus),
      super(segmentId: segmentId, timestampUs: timestampUs);

  /// What went wrong.
  final MockingbirdStatus status;
}
