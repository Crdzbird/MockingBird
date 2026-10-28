part of 'mockingbird_engine.dart';

/// Speaker verification on a [MockingbirdEngine]: whether each utterance
/// comes from one enrolled person, reported as [MockingbirdSpeakerEvent]s.
/// Needs 16 kHz input and voice detection; it recognises a voice, it does
/// not separate overlapping ones.
extension MockingbirdEngineSpeaker on MockingbirdEngine {
  /// Names the speaker model, from `MockingbirdModels.speakerModelPath`.
  /// Only while stopped.
  void setSpeakerModel(String path) {
    _ensureLive();
    MockingbirdException.check(
      _platform.setSpeakerModel(_handle, path),
      'mockingbird_set_speaker_model',
    );
  }

  /// The voice print of 16 kHz mono [samples], at least 0.5 s; 3 to 10 s of
  /// one person speaking gives the steadiest print. Keep it to enroll the
  /// same person again later without recording.
  Float32List speakerEmbedding(Int16List samples) {
    _ensureLive();
    final r = _platform.speakerEmbedding(_handle, samples);
    MockingbirdException.check(r.status, 'mockingbird_speaker_embedding');
    return r.embedding;
  }

  /// Compares every utterance with [embedding] from now on; [threshold] is
  /// the similarity that counts as a match. Null stops. Allowed while
  /// running.
  void setTargetSpeaker(Float32List? embedding, {double threshold = 0.5}) {
    _ensureLive();
    MockingbirdException.check(
      _platform.setTargetSpeaker(_handle, embedding, threshold),
      'mockingbird_set_target_speaker',
    );
  }
}
