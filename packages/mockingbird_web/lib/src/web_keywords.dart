import 'dart:convert';
import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_keyword_api.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

/// Keyword spotting setup in the module. Model paths are in its memory file
/// system, as `storeModel` returns them.
final class WebKeywords {
  /// Wraps the loaded module.
  const factory WebKeywords(MockingbirdModule m) = WebKeywords._;

  const WebKeywords._(this._m);

  final MockingbirdModule _m;

  /// Names the two front-end models; null uses the engine's defaults.
  int frontend(WebEngineHandle h, String? melspectrogram, String? embedding) {
    final mel = _string(melspectrogram);
    final emb = _string(embedding);
    try {
      return MockingbirdKeywordApi(_m).setFrontend(h.pointer, mel, emb);
    } finally {
      _m
        ..free(mel)
        ..free(emb);
    }
  }

  /// Registers the keyword model at [path].
  MockingbirdKeywordResult add(WebEngineHandle h, String path, double t) {
    final p = _string(path);
    final id = _m.malloc(4);
    try {
      final s = MockingbirdKeywordApi(_m).add(h.pointer, p, t, id);
      return (status: s, id: _m.heap32.toDart[id >> 2]);
    } finally {
      _m
        ..free(p)
        ..free(id);
    }
  }

  /// Removes every keyword.
  int clear(WebEngineHandle h) => MockingbirdKeywordApi(_m).clear(h.pointer);

  int _string(String? s) {
    if (s == null) return 0;
    final bytes = Uint8List.fromList([...utf8.encode(s), 0]);
    final p = _m.malloc(bytes.length);
    _m.heapU8.toDart.setAll(p, bytes);
    return p;
  }
}
