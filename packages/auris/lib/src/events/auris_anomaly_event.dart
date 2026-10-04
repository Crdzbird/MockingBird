part of 'auris_event.dart';

/// A sudden or unusual sound tripped the anomaly gate.
final class AurisAnomalyEvent extends AurisEvent {
  const AurisAnomalyEvent._(
    int segmentId,
    int timestampUs,
    this.reasonMask,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// Raw reason bits; prefer [reasons].
  final int reasonMask;

  /// How strongly the gate fired, 0..1.
  final double confidence;

  /// Which statistics tripped the gate.
  Set<AurisAnomalyReason> get reasons =>
      AurisAnomalyReason.fromMask(reasonMask);
}
