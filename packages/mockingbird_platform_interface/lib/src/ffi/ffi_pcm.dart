import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The PCM push and tap paths over the FFI bindings.
final class FfiPcm {
  /// Wraps [_bindings].
  const FfiPcm(this._bindings);

  final MockingbirdBindings _bindings;

  /// Pushes samples through a native buffer.
  int push(FfiEngineHandle h, Int16List samples, int captureTsUs) {
    final buf = calloc<ffi.Int16>(samples.length);
    try {
      buf.asTypedList(samples.length).setAll(0, samples);
      return _bindings
          .mockingbird_push_pcm(h.pointer, buf, samples.length, captureTsUs)
          .value;
    } finally {
      calloc.free(buf);
    }
  }

  /// Reads the PCM tap into a Dart-owned list.
  MockingbirdPcmResult read(FfiEngineHandle h, int maxSamples) {
    final buf = calloc<ffi.Int16>(maxSamples);
    final written = calloc<ffi.Uint32>();
    try {
      final s = _bindings
          .mockingbird_read_pcm(h.pointer, buf, maxSamples, written)
          .value;
      if (s != MockingbirdStatusCode.ok) {
        return (status: s, samples: Int16List(0));
      }
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
