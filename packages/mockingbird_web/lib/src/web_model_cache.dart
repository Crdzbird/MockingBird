import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:web/web.dart' as web;

/// Keeps downloaded models in the browser's Cache Storage, keyed by their
/// checksum, so a later visit opens them without the network. Pages that are
/// not a secure context have no Cache Storage and download every time.
final class WebModelCache {
  /// Creates the cache helper.
  const factory WebModelCache() = WebModelCache._;

  const WebModelCache._();

  static const _name = 'mockingbird-models';

  /// The cached bytes of [spec], if any.
  Future<Uint8List?> read(MockingbirdModelSpec spec) async {
    if (!web.window.isSecureContext) return null;
    final cache = await web.window.caches.open(_name).toDart;
    final hit = await cache.match(_key(spec).toJS).toDart;
    if (hit == null) return null;
    final buffer = await hit.arrayBuffer().toDart;
    return buffer.toDart.asUint8List();
  }

  /// Keeps [bytes] for [spec].
  Future<void> write(MockingbirdModelSpec spec, Uint8List bytes) async {
    if (!web.window.isSecureContext) return;
    final cache = await web.window.caches.open(_name).toDart;
    await cache.put(_key(spec).toJS, web.Response(bytes.toJS)).toDart;
  }

  static String _key(MockingbirdModelSpec spec) =>
      'https://mockingbird.models/${spec.sha256}/${spec.fileName}';
}
