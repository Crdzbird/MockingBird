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

  static String? _neuralVadPath;
  static String? _soundClassPath;
  static List<String>? _yamnetLabels;

  /// Where the engine opens the Silero VAD model, for
  /// `MockingbirdVadConfig.modelPath`.
  static Future<String> neuralVadPath() async =>
      _neuralVadPath ??= await _materialise('silero_vad');

  /// Where the engine opens the YAMNet model, for
  /// `MockingbirdSoundClassConfig.modelPath`.
  static Future<String> soundClassPath() async =>
      _soundClassPath ??= await _materialise('yamnet_3s');

  /// YAMNet's 521 AudioSet labels, indexed by class id.
  static Future<List<String>> yamnetLabels() async => _yamnetLabels ??=
      _parseLabels(await rootBundle.loadString(yamnetLabelsAsset));

  static List<String> _parseLabels(String text) => [
    for (final line in text.split('\n'))
      if (line.trim() case final label when label.isNotEmpty)
        label.substring(label.indexOf(' ') + 1),
  ];

  static Future<String> _materialise(String model) async {
    final filename = kIsWeb ? '$model.ort' : '$model.onnx';
    final data = await rootBundle.load('packages/mockingbird/assets/$filename');
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
