part of 'auris_event.dart';

/// YAMNet recognised a sound.
final class AurisSoundClassEvent extends AurisEvent {
  const AurisSoundClassEvent._(
    int segmentId,
    int timestampUs,
    this.classId,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The AudioSet class index, 0..520; see `AurisModels.yamnetLabels`.
  final int classId;

  /// Top-1 probability, 0..1.
  final double confidence;

  /// The broad category of [classId].
  AurisSoundCategory get category => AurisSoundCategory.of(classId);

  /// True if [classId] is a human voice in any language.
  bool get isHumanVoice => AurisSoundCategory.isHumanVoice(classId);
}
