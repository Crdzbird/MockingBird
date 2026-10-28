import 'dart:convert';
import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_record_api.dart';

/// Puts model bytes in the module's in-memory file system, where the engine
/// opens them like files on disk.
final class WebModelStore {
  /// Wraps the loaded module.
  const factory WebModelStore(MockingbirdModule m) = WebModelStore._;

  const WebModelStore._(this._m);

  final MockingbirdModule _m;

  /// Stores [bytes] as `/models/<name>` and returns that path.
  String store(String name, Uint8List bytes) {
    final path = '/models/$name';
    final cPath = Uint8List.fromList([...utf8.encode(path), 0]);
    final pathPtr = _m.malloc(cPath.length);
    final dataPtr = _m.malloc(bytes.isEmpty ? 1 : bytes.length);
    try {
      _m.heapU8.toDart
        ..setAll(pathPtr, cPath)
        ..setAll(dataPtr, bytes);
      final s = MockingbirdRecordApi(
        _m,
      ).storeFile(pathPtr, dataPtr, bytes.length);
      if (s != 0) {
        throw StateError('mockingbird_web: could not store $path ($s)');
      }
      return path;
    } finally {
      _m
        ..free(pathPtr)
        ..free(dataPtr);
    }
  }
}
