import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:auris_platform_interface/src/auris_platform.dart';
import 'package:auris_platform_interface/src/auris_status_code.dart';
import 'package:auris_platform_interface/src/bindings.dart';
import 'package:auris_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:ffi/ffi.dart';

/// The PCM push and tap paths over the FFI bindings.
final class FfiPcm {
  /// Wraps [_bindings].
  const FfiPcm(this._bindings);

  final AurisBindings _bindings;

  /// Pushes samples through a native buffer.
  int push(FfiEngineHandle h, Int16List samples, int captureTsUs) {
    final buf = calloc<ffi.Int16>(samples.length);
    try {
      buf.asTypedList(samples.length).setAll(0, samples);
      return _bindings
          .auris_push_pcm(h.pointer, buf, samples.length, captureTsUs)
          .value;
    } finally {
      calloc.free(buf);
    }
  }

  /// Reads the PCM tap into a Dart-owned list.
  AurisPcmResult read(FfiEngineHandle h, int maxSamples) {
    final buf = calloc<ffi.Int16>(maxSamples);
    final written = calloc<ffi.Uint32>();
    try {
      final s =
          _bindings.auris_read_pcm(h.pointer, buf, maxSamples, written).value;
      if (s != AurisStatusCode.ok) return (status: s, samples: Int16List(0));
      return (
        status: s,
        samples: Int16List.fromList(buf.asTypedList(written.value)),
      );
    } finally {
      calloc
        ..free(buf)
        ..free(written);
    }
  }
}
