import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_device.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// Input device listing and selection over the FFI bindings.
final class FfiDevices {
  /// Wraps [_bindings].
  const factory FfiDevices(MockingbirdBindings bindings) = FfiDevices._;

  const FfiDevices._(this._bindings);

  static const int _capacity = 64;

  final MockingbirdBindings _bindings;

  /// Lists the capture devices of `mockingbird_backend_t` [backend].
  MockingbirdDevicesResult list(int backend) {
    final out = calloc<mockingbird_device_t>(_capacity);
    final count = calloc<Uint32>();
    try {
      final s = _bindings
          .mockingbird_list_input_devices(
            mockingbird_backend.fromValue(backend),
            out,
            _capacity,
            count,
          )
          .value;
      if (s != MockingbirdStatusCode.ok) return (status: s, devices: const []);
      final n = count.value < _capacity ? count.value : _capacity;
      return (
        status: s,
        devices: [for (var i = 0; i < n; i++) _device(out[i])],
      );
    } finally {
      calloc
        ..free(out)
        ..free(count);
    }
  }

  /// Selects the device the next start opens; null follows the default.
  int select(FfiEngineHandle h, String? id) {
    final native = id?.toNativeUtf8();
    try {
      return _bindings
          .mockingbird_set_input_device(h.pointer, native?.cast() ?? nullptr)
          .value;
    } finally {
      if (native != null) calloc.free(native);
    }
  }

  static MockingbirdNativeDevice _device(mockingbird_device d) =>
      MockingbirdNativeDevice(
        id: _text(d.id),
        name: _text(d.name),
        isDefault: d.is_default != 0,
      );

  static String _text(Array<Char> chars) {
    final bytes = <int>[];
    for (var i = 0; i < 256 && chars[i] != 0; i++) {
      bytes.add(chars[i] & 0xFF);
    }
    return utf8.decode(bytes, allowMalformed: true);
  }
}
