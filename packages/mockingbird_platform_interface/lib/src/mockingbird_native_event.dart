/// A flat mirror of `mockingbird_event_t`.
final class MockingbirdNativeEvent {
  /// Creates an event record.
  const MockingbirdNativeEvent({
    required this.type,
    required this.segmentId,
    required this.timestampUs,
    required this.reasonMask,
    required this.confidence,
  });

  /// The `mockingbird_event_type_t` value as an integer.
  final int type;

  /// Segment the event belongs to, 0 when none.
  final int segmentId;

  /// Deterministic timestamp in microseconds.
  final int timestampUs;

  /// Unsigned 32-bit payload whose meaning depends on [type].
  final int reasonMask;

  /// Detector confidence in `0..1`, when the type carries one.
  final double confidence;
}
