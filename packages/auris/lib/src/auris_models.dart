import 'dart:io' show File;

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/services.dart' show rootBundle;
import 'package:path_provider/path_provider.dart';

/// The ONNX models and labels bundled with the plugin, materialised where the
/// native engine can open them. Every call is idempotent and cached.
final class AurisModels {
  const AurisModels._();

  /// Asset key of the Silero VAD model.
  static const neuralVadAsset = 'packages/auris/assets/silero_vad.onnx';

  /// Asset key of the YAMNet sound classifier.
  static const soundClassAsset = 'packages/auris/assets/yamnet_3s.onnx';

  /// Asset key of YAMNet's class labels, one `<index> <label>` per line.
  static const yamnetLabelsAsset = 'packages/auris/assets/yamnet_class_map.txt';

  static String? _neuralVadPath;
  static String? _soundClassPath;
  static List<String>? _yamnetLabels;

  /// On-disk path of the Silero VAD model, for `AurisVadConfig.modelPath`.
  static Future<String> neuralVadPath() async =>
      _neuralVadPath ??= await _extract(neuralVadAsset, 'silero_vad.onnx');

  /// On-disk path of the YAMNet model, for `AurisSoundClassConfig.modelPath`.
  static Future<String> soundClassPath() async =>
      _soundClassPath ??= await _extract(soundClassAsset, 'yamnet_3s.onnx');

  /// YAMNet's 521 AudioSet labels, indexed by class id.
  static Future<List<String>> yamnetLabels() async => _yamnetLabels ??=
      _parseLabels(await rootBundle.loadString(yamnetLabelsAsset));

  static List<String> _parseLabels(String text) => [
    for (final line in text.split('\n'))
      if (line.trim() case final label when label.isNotEmpty)
        label.substring(label.indexOf(' ') + 1),
  ];

  static Future<String> _extract(String asset, String filename) async {
    if (kIsWeb) {
      throw UnsupportedError(
        'The neural detectors are not available on the web; use the fast '
        'VAD tier and the anomaly gate.',
      );
    }
    final dir = await getApplicationSupportDirectory();
    final file = File('${dir.path}/$filename');
    if (file.existsSync() && file.lengthSync() > 0) return file.path;
    final data = await rootBundle.load(asset);
    await file.writeAsBytes(
      data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
      flush: true,
    );
    return file.path;
  }
}
