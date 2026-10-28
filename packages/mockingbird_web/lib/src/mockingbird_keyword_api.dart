import 'dart:js_interop';

/// The module's keyword spotting entry points.
extension type MockingbirdKeywordApi(JSObject _) implements JSObject {
  /// Names the keyword front-end models; 0 means the default.
  @JS('_mockingbird_set_keyword_frontend')
  external int setFrontend(int engine, int melspectrogram, int embedding);

  /// Registers a keyword model and writes its id to [outId].
  @JS('_mockingbird_add_keyword')
  external int add(int engine, int path, double threshold, int outId);

  /// Removes every keyword.
  @JS('_mockingbird_clear_keywords')
  external int clear(int engine);
}
