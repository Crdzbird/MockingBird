import 'dart:io' show File;

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/services.dart' show rootBundle;
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:path_provider/path_provider.dart';

/// Copies models from the asset bundle to where the engine can open them:
/// the app's support directory on native platforms, the module's memory on
/// the web. Each asset is copied once per run, and again only if it changed.
final class AssetModels {
  /// Creates the cache.
  AssetModels();

  final Map<String, String> _paths = {};

  /// The engine-readable path of asset [key].
  Future<String> path(String key) async =>
      _paths[key] ??= await _materialise(key);

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
