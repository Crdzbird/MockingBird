import 'dart:js_interop';

/// The module's speaker verification entry points.
extension type MockingbirdSpeakerApi(JSObject _) implements JSObject {
  /// Names the speaker embedding model; 0 means the default.
  @JS('_mockingbird_set_speaker_model')
  external int setModel(int engine, int path);

  /// Writes the voice print of [count] samples to [out].
  @JS('_mockingbird_speaker_embedding')
  external int embedding(
    int engine,
    int samples,
    int count,
    int out,
    int capacity,
    int outDim,
  );

  /// Sets the target voice print; 0 stops.
  @JS('_mockingbird_set_target_speaker')
  external int setTarget(int engine, int embedding, int dim, double threshold);
}
