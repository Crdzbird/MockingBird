part of 'mockingbird_event.dart';

/// An utterance was compared with the target speaker set with
/// `MockingbirdEngine.setTargetSpeaker`.
final class MockingbirdSpeakerEvent extends MockingbirdEvent {
  const MockingbirdSpeakerEvent._(
    int segmentId,
    int timestampUs,
    this.isTarget,
    this.similarity,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// Whether [similarity] reached the threshold.
  final bool isTarget;

  /// How alike the voices are, 0..1.
  final double similarity;
}
