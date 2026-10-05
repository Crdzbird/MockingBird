import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_config.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';

/// Engine creation, destruction, retuning and the two string queries.
final class FfiEngines {
  /// Wraps [_bindings].
  const FfiEngines(this._bindings);

  final MockingbirdBindings _bindings;

  /// The linked engine version.
  String version() =>
      _bindings.mockingbird_version_string().cast<Utf8>().toDartString();

  /// The engine's name for [status].
  String statusName(int status) =>
      _bindings.mockingbird_status_str(status).cast<Utf8>().toDartString();

  /// Creates an engine from [config].
  MockingbirdCreateResult create(MockingbirdNativeConfig config) {
    final native = FfiConfig.from(_bindings, config);
    final out = calloc<ffi.Pointer<mockingbird_engine_t>>();
    try {
      final s = _bindings.mockingbird_create(native.pointer, out).value;
      if (s != MockingbirdStatusCode.ok) return (status: s, handle: null);
      return (status: s, handle: FfiEngineHandle(out.value));
    } finally {
      native.free();
      calloc.free(out);
    }
  }

  /// Destroys the engine and frees its scratch records.
  void destroy(FfiEngineHandle h) {
    _bindings.mockingbird_destroy(h.pointer);
    h.free();
  }

  /// Retunes the running engine.
  int updateConfig(FfiEngineHandle h, MockingbirdNativeConfig config) {
    final native = FfiConfig.from(_bindings, config);
    try {
      return _bindings
          .mockingbird_update_config(h.pointer, native.pointer)
          .value;
    } finally {
      native.free();
    }
  }
}
