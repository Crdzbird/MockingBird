import 'dart:js_interop';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/web_model_cache.dart';
import 'package:web/web.dart' as web;

/// Fetches models over HTTP in the browser: several attempts per source with
/// growing pauses, a source that serves the wrong bytes skipped, and the
/// result checked against its size and SHA-256 before it is used or cached.
final class WebModelDownload {
  /// Creates the downloader.
  const factory WebModelDownload() = WebModelDownload._;

  const WebModelDownload._();

  static const _attempts = 3;
  static const _stall = Duration(seconds: 30);

  /// The verified bytes of [spec], from the cache or the network.
  Future<Uint8List> fetch(
    MockingbirdModelSpec spec,
    MockingbirdDownloadProgress? onProgress,
  ) async {
    const cache = WebModelCache();
    final cached = await cache.read(spec);
    if (cached != null && _matches(cached, spec)) return cached;
    Object? last;
    for (final url in spec.urls) {
      for (var attempt = 0; attempt < _attempts; attempt++) {
        if (attempt > 0) {
          await Future<void>.delayed(Duration(seconds: 1 << (attempt - 1)));
        }
        final Uint8List bytes;
        try {
          bytes = await _get(url, spec.bytes, onProgress);
        } on Object catch (e) {
          last = e;
          continue;
        }
        if (_matches(bytes, spec)) {
          await cache.write(spec, bytes);
          return bytes;
        }
        last = StateError('$url served a file with the wrong checksum');
        break;
      }
    }
    throw MockingbirdDownloadException(spec.fileName, last);
  }

  static bool _matches(Uint8List bytes, MockingbirdModelSpec spec) =>
      bytes.length == spec.bytes &&
      sha256.convert(bytes).toString() == spec.sha256;

  static Future<Uint8List> _get(
    Uri url,
    int total,
    MockingbirdDownloadProgress? onProgress,
  ) async {
    final res = await web.window
        .fetch(url.toString().toJS)
        .toDart
        .timeout(_stall);
    if (!res.ok || res.body == null) throw StateError('HTTP ${res.status}');
    final reader = res.body!.getReader() as web.ReadableStreamDefaultReader;
    final out = BytesBuilder(copy: false);
    while (true) {
      final chunk = await reader.read().toDart.timeout(_stall);
      if (chunk.done) break;
      out.add((chunk.value! as JSUint8Array).toDart);
      if (out.length > total) throw StateError('$url sent too many bytes');
      onProgress?.call(out.length, total);
    }
    return out.takeBytes();
  }
}
