part of 'mockingbird_engine.dart';

/// Keyword spotting on a [MockingbirdEngine]: wake words and short commands
/// with openWakeWord-compatible models, heard as [MockingbirdKeywordEvent]s.
/// 16 kHz only. Set up while stopped; changes apply from the next [start].
extension MockingbirdEngineKeywords on MockingbirdEngine {
  /// Names the mel spectrogram and speech embedding models every keyword
  /// shares, from `MockingbirdModels.keywordMelspectrogramPath` and
  /// `MockingbirdModels.keywordEmbeddingPath`.
  void setKeywordFrontend({
    required String melspectrogram,
    required String embedding,
  }) {
    _ensureLive();
    MockingbirdException.check(
      _platform.setKeywordFrontend(_handle, melspectrogram, embedding),
      'mockingbird_set_keyword_frontend',
    );
  }

  /// Listens for the keyword model at [modelPath] and returns the id its
  /// events carry. [threshold] is the score that fires it. After firing, a
  /// keyword stays quiet for one second and until its score drops below
  /// [threshold]. Up to 16 keywords.
  int addKeyword(String modelPath, {double threshold = 0.5}) {
    _ensureLive();
    final r = _platform.addKeyword(_handle, modelPath, threshold);
    MockingbirdException.check(r.status, 'mockingbird_add_keyword');
    return r.id;
  }

  /// Stops listening for every keyword.
  void clearKeywords() {
    _ensureLive();
    MockingbirdException.check(
      _platform.clearKeywords(_handle),
      'mockingbird_clear_keywords',
    );
  }
}
