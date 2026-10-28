import 'dart:convert';
import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_speaker_api.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

/// Speaker verification in the module. The model path is in its memory file
/// system, as `storeModel` returns it.
final class WebSpeaker {
  /// Wraps the loaded module.
  const factory WebSpeaker(MockingbirdModule m) = WebSpeaker._;

  const WebSpeaker._(this._m);

  static const _capacity = 4096;

  final MockingbirdModule _m;

  MockingbirdSpeakerApi get _api => MockingbirdSpeakerApi(_m);

  /// Names the speaker embedding model; null uses the default.
  int model(WebEngineHandle h, String? path) {
    final bytes = path == null ? null : [...utf8.encode(path), 0];
    final p = bytes == null ? 0 : _m.malloc(bytes.length);
    if (bytes != null) _m.heapU8.toDart.setAll(p, bytes);
    try {
      return _api.setModel(h.pointer, p);
    } finally {
      _m.free(p);
    }
  }

  /// The voice print of [samples].
  MockingbirdSpeakerEmbeddingResult embedding(
    WebEngineHandle h,
    Int16List samples,
  ) {
    final input = _m.malloc(samples.length * 2 + 2);
    final out = _m.malloc(_capacity * 4);
    final dim = _m.malloc(4);
    try {
      _m.heap16.toDart.setAll(input >> 1, samples);
      final s = _api.embedding(
        h.pointer,
        input,
        samples.length,
        out,
        _capacity,
        dim,
      );
      final n = _m.heap32.toDart[dim >> 2].clamp(0, _capacity);
      final view = _m.heapU8.toDart.buffer.asFloat32List(out, n);
      return (status: s, embedding: Float32List.fromList(view));
    } finally {
      _m
        ..free(input)
        ..free(out)
        ..free(dim);
    }
  }

  /// Sets the target voice print; null stops.
  int target(WebEngineHandle h, Float32List? embedding, double threshold) {
    final n = embedding?.length ?? 0;
    final p = embedding == null ? 0 : _m.malloc(n * 4);
    if (embedding != null) {
      _m.heapU8.toDart.buffer.asFloat32List(p, n).setAll(0, embedding);
    }
    try {
      return _api.setTarget(h.pointer, p, n, threshold);
    } finally {
      _m.free(p);
    }
  }
}
