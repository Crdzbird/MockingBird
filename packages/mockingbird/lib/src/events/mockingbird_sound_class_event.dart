part of 'mockingbird_event.dart';

/// YAMNet recognised a sound.
final class MockingbirdSoundClassEvent extends MockingbirdEvent {
  const MockingbirdSoundClassEvent._(
    int segmentId,
    int timestampUs,
    this.classId,
    this.confidence,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The AudioSet class index, 0..520; see `MockingbirdModels.yamnetLabels`.
  final int classId;

  /// Top-1 probability, 0..1.
  final double confidence;

  /// The broad category of [classId].
  MockingbirdSoundCategory get category => MockingbirdSoundCategory.of(classId);

  /// True if [classId] is a human voice in any language.
  bool get isHumanVoice => MockingbirdSoundCategory.isHumanVoice(classId);
}
