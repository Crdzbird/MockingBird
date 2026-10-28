import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_record_api.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

/// The PCM push and tap paths over the WebAssembly module.
final class WebPcm {
  /// Wraps the loaded module.
  const factory WebPcm(MockingbirdModule m) = WebPcm._;

  const WebPcm._(this._m);

  final MockingbirdModule _m;

  /// Copies samples into linear memory and pushes them.
  int push(WebEngineHandle h, Int16List samples, int captureTsUs) {
    final buf = _m.malloc(samples.length * 2);
    try {
      _m.heap16.toDart.setAll(buf >> 1, samples);
      return MockingbirdRecordApi(
        _m,
      ).pushPcm(h.pointer, buf, samples.length, captureTsUs.toDouble());
    } finally {
      _m.free(buf);
    }
  }

  /// Reads the PCM tap into a Dart-owned list.
  MockingbirdPcmResult read(WebEngineHandle h, int maxSamples) {
    final buf = _m.malloc(maxSamples * 2);
    final written = _m.malloc(4);
    try {
      final s = _m.readPcm(h.pointer, buf, maxSamples, written);
      if (s != MockingbirdStatusCode.ok) {
        return (status: s, samples: Int16List(0));
      }
      final n = _m.heap32.toDart[written >> 2];
      final start = buf >> 1;
      return (
        status: s,
        samples: Int16List.fromList(_m.heap16.toDart.sublist(start, start + n)),
      );
    } finally {
      _m
        ..free(buf)
        ..free(written);
    }
  }
}
