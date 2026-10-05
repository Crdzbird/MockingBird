import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

/// The PCM push and tap paths over the WebAssembly module.
final class WebPcm {
  const WebPcm(this._m);

  final AurisModule _m;

  int push(WebEngineHandle h, Int16List samples, int captureTsUs) {
    final buf = _m.malloc(samples.length * 2);
    try {
      _m.HEAP16.toDart.setAll(buf >> 1, samples);
      return _m.auris_web_push_pcm(
        h.pointer,
        buf,
        samples.length,
        captureTsUs.toDouble(),
      );
    } finally {
      _m.free(buf);
    }
  }

  AurisPcmResult read(WebEngineHandle h, int maxSamples) {
    final buf = _m.malloc(maxSamples * 2);
    final written = _m.malloc(4);
    try {
      final s = _m.auris_read_pcm(h.pointer, buf, maxSamples, written);
      if (s != AurisStatusCode.ok) return (status: s, samples: Int16List(0));
      final n = _m.HEAP32.toDart[written >> 2];
      final start = buf >> 1;
      return (
        status: s,
        samples: Int16List.fromList(_m.HEAP16.toDart.sublist(start, start + n)),
      );
    } finally {
      _m
        ..free(buf)
        ..free(written);
    }
  }
}
