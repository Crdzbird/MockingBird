import 'package:mockingbird/src/asset_models.dart';
import 'package:mockingbird/src/model_downloads.dart';
import 'package:mockingbird/src/yamnet_labels.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// The models the engine opens, where it can open them. Silero VAD and the
/// keyword front end ship as assets; YAMNet and the speaker model download on
/// first use from the pinned `MockingbirdModelCatalog`, checked against their
/// SHA-256 and kept, so later calls work offline. Every model is in ONNX
/// Runtime's `.ort` format, the only one the reduced ONNX Runtime the plugin
/// links reads. Native platforms keep files in the app's support directory;
/// the web keeps them in the module's memory and the browser's cache. Every
/// call is idempotent.
final class MockingbirdModels {
  const MockingbirdModels._();

  /// Asset key of the Silero VAD model.
  static const neuralVadAsset = 'packages/mockingbird/assets/silero_vad.ort';

  /// Asset key of YAMNet's class labels, one `<index> <label>` per line.
  static const yamnetLabelsAsset =
      'packages/mockingbird/assets/yamnet_class_map.txt';

  static final AssetModels _assets = AssetModels();
  static final ModelDownloads _downloads = ModelDownloads();

  /// Servers tried before the pinned sources, each a directory URL ending in
  /// `/` that holds the files under their catalog names. Set it to serve the
  /// models from your own host or CDN.
  static List<Uri> mirrors = const [];
  static final YamnetLabels _labels = YamnetLabels();

  /// Where the engine opens the Silero VAD model, for
  /// `MockingbirdVadConfig.modelPath`.
  static Future<String> neuralVadPath() => _model('silero_vad');

  /// Where the engine opens the YAMNet model, for
  /// `MockingbirdSoundClassConfig.modelPath`. Downloads 5 MB on first use,
  /// reporting [onProgress]; throws `MockingbirdDownloadException` when no
  /// source can be reached.
  static Future<String> soundClassPath({
    MockingbirdDownloadProgress? onProgress,
  }) => _downloads.path(MockingbirdModelCatalog.yamnetOrt, mirrors, onProgress);

  /// Where the engine opens the speaker model, for
  /// `MockingbirdEngine.setSpeakerModel`. Downloads 8.4 MB on first use.
  static Future<String> speakerModelPath({
    MockingbirdDownloadProgress? onProgress,
  }) =>
      _downloads.path(MockingbirdModelCatalog.speakerOrt, mirrors, onProgress);

  /// Where the engine opens the keyword front end's mel spectrogram model,
  /// for `MockingbirdEngine.setKeywordFrontend`.
  static Future<String> keywordMelspectrogramPath() => _model('melspectrogram');

  /// Where the engine opens the keyword front end's speech embedding model,
  /// for `MockingbirdEngine.setKeywordFrontend`.
  static Future<String> keywordEmbeddingPath() => _model('embedding_model');

  /// Materialises any model in the app's asset bundle, such as a keyword
  /// model for `MockingbirdEngine.addKeyword`, and returns its path. Bundle
  /// the model's `.ort` conversion.
  static Future<String> fromAsset(String key) => _assets.path(key);

  static Future<String> _model(String name) =>
      fromAsset('packages/mockingbird/assets/$name.ort');

  /// YAMNet's 521 AudioSet labels, indexed by class id.
  static Future<List<String>> yamnetLabels() => _labels.load(yamnetLabelsAsset);
}
