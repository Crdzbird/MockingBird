import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:path_provider/path_provider.dart';

/// Runs model downloads for `MockingbirdModels`: one at a time per model, so
/// concurrent callers share it, and forgotten after a failure, so the next
/// call tries again.
final class ModelDownloads {
  /// Creates the registry.
  ModelDownloads();

  final Map<String, Future<String>> _running = {};

  /// The path of [spec], downloaded under the app's support directory, or
  /// into the module's memory on the web, if it is not there yet.
  Future<String> path(
    MockingbirdModelSpec spec,
    List<Uri> mirrors,
    MockingbirdDownloadProgress? onProgress,
  ) => _running.putIfAbsent(
    spec.sha256,
    () => _fetch(spec.withMirrors(mirrors), onProgress),
  );

  Future<String> _fetch(
    MockingbirdModelSpec spec,
    MockingbirdDownloadProgress? onProgress,
  ) async {
    try {
      final dir = kIsWeb ? '' : (await getApplicationSupportDirectory()).path;
      return await MockingbirdPlatform.instance.downloadModel(
        spec,
        directory: dir,
        onProgress: onProgress,
      );
    } on Object {
      _running.removeWhere((key, _) => key == spec.sha256);
      rethrow;
    }
  }
}
