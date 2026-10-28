import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_http_transfer.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_download_exception.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_download_progress.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_model_spec.dart';

/// Downloads models into a directory. A file is only ever renamed into place
/// after its size and SHA-256 match, so a present file is a good one. Each
/// source gets several attempts with growing pauses, resuming where the last
/// one stopped; a source that serves the wrong bytes is skipped.
final class FfiModelDownloader {
  /// Creates the downloader.
  const factory FfiModelDownloader({
    FfiHttpTransfer transfer,
    int attempts,
    Duration retryDelay,
  }) = FfiModelDownloader._;

  const FfiModelDownloader._({
    this.transfer = const FfiHttpTransfer(),
    this.attempts = 3,
    this.retryDelay = const Duration(seconds: 1),
  });

  /// Fetches one source.
  final FfiHttpTransfer transfer;

  /// Attempts per source.
  final int attempts;

  /// The pause before the second attempt; it doubles after each failure.
  final Duration retryDelay;

  /// Returns the path of [spec] under [directory], downloading it if needed.
  Future<String> fetch(
    MockingbirdModelSpec spec,
    String directory,
    MockingbirdDownloadProgress? onProgress,
  ) async {
    final dir = await Directory(
      '$directory/mockingbird_models',
    ).create(recursive: true);
    final target = File(
      '${dir.path}/${spec.sha256.substring(0, 16)}_${spec.fileName}',
    );
    if (target.existsSync() && target.lengthSync() == spec.bytes) {
      return target.path;
    }
    final part = File('${target.path}.part');
    Object? last;
    for (final url in spec.urls) {
      for (var attempt = 0; attempt < attempts; attempt++) {
        if (attempt > 0) {
          await Future<void>.delayed(retryDelay * (1 << (attempt - 1)));
        }
        try {
          await transfer.fetch(url, part, spec.bytes, onProgress);
        } on Object catch (e) {
          last = e;
          continue;
        }
        if (await _matches(part, spec)) {
          return (await part.rename(target.path)).path;
        }
        last = StateError('$url served a file with the wrong checksum');
        await part.delete();
        break;
      }
    }
    throw MockingbirdDownloadException(spec.fileName, last);
  }

  static Future<bool> _matches(File file, MockingbirdModelSpec spec) async {
    if (file.lengthSync() != spec.bytes) return false;
    final digest = await sha256.bind(file.openRead()).first;
    return digest.toString() == spec.sha256;
  }
}
