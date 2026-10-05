part of 'mockingbird_event.dart';

/// An event type this version of the Dart package does not know yet.
final class MockingbirdUnknownEvent extends MockingbirdEvent {
  const MockingbirdUnknownEvent._(
    int segmentId,
    int timestampUs,
    this.nativeType,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The native `mockingbird_event_type_t` value.
  final int nativeType;
}
