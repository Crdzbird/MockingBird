import 'dart:io' show File;

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/services.dart' show rootBundle;
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:path_provider/path_provider.dart';

/// The ONNX models and labels bundled with the plugin, materialised where the
/// engine can open them. Native platforms extract the `.onnx` files to the
/// app's support directory; the web stores the `.ort` builds of the same
/// models in the module's memory. Every call is idempotent and cached.
final class MockingbirdModels {
  const MockingbirdModels._();

  /// Asset key of the Silero VAD model.
  static const neuralVadAsset = 'packages/mockingbird/assets/silero_vad.onnx';

  /// Asset key of the YAMNet sound classifier.
  static const soundClassAsset = 'packages/mockingbird/assets/yamnet_3s.onnx';

  /// Asset key of YAMNet's class labels, one `<index> <label>` per line.
  static const yamnetLabelsAsset =
      'packages/mockingbird/assets/yamnet_class_map.txt';

  static final Map<String, String> _paths = {};
  static List<String>? _yamnetLabels;

  /// Where the engine opens the Silero VAD model, for
  /// `MockingbirdVadConfig.modelPath`.
  static Future<String> neuralVadPath() => _model('silero_vad');

  /// Where the engine opens the YAMNet model, for
  /// `MockingbirdSoundClassConfig.modelPath`.
  static Future<String> soundClassPath() => _model('yamnet_3s');

  /// Where the engine opens the keyword front end's mel spectrogram model,
  /// for `MockingbirdEngine.setKeywordFrontend`.
  static Future<String> keywordMelspectrogramPath() => _model('melspectrogram');

  /// Where the engine opens the keyword front end's speech embedding model,
  /// for `MockingbirdEngine.setKeywordFrontend`.
  static Future<String> keywordEmbeddingPath() => _model('embedding_model');

  /// Materialises any model in the app's asset bundle, such as a keyword
  /// model for `MockingbirdEngine.addKeyword`, and returns its path. On the
  /// web, bundle and pass the `.ort` build of the model.
  static Future<String> fromAsset(String key) async =>
      _paths[key] ??= await _materialise(key);

  static Future<String> _model(String name) =>
      fromAsset('packages/mockingbird/assets/$name.${kIsWeb ? 'ort' : 'onnx'}');

  /// YAMNet's 521 AudioSet labels, indexed by class id.
  static Future<List<String>> yamnetLabels() async => _yamnetLabels ??=
      _parseLabels(await rootBundle.loadString(yamnetLabelsAsset));

  static List<String> _parseLabels(String text) => [
    for (final line in text.split('\n'))
      if (line.trim() case final label when label.isNotEmpty)
        label.substring(label.indexOf(' ') + 1),
  ];

  static Future<String> _materialise(String key) async {
    final filename = key.substring(key.lastIndexOf('/') + 1);
    final data = await rootBundle.load(key);
    final bytes = data.buffer.asUint8List(
      data.offsetInBytes,
      data.lengthInBytes,
    );
    if (kIsWeb) {
      return await MockingbirdPlatform.instance.storeModel(filename, bytes);
    }
    final dir = await getApplicationSupportDirectory();
    final file = File('${dir.path}/$filename');
    if (file.existsSync() && file.lengthSync() == bytes.length) {
      return file.path;
    }
    await file.writeAsBytes(bytes, flush: true);
    return file.path;
  }
}
