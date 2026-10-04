import 'dart:ffi' as ffi;

import 'package:auris/src/auris_status.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';

/// Facts about the linked native engine.
final class Auris {
  const Auris._();

  /// The `X.Y.Z` version of the native engine. Reading it also proves the
  /// native library is linked and reachable through `dart:ffi`.
  static String get version => _string(_bindings.auris_version_string());

  /// The engine's own name for a [status].
  static String statusName(AurisStatus status) =>
      _string(_bindings.auris_status_str(status.nativeValue));

  static AurisBindings get _bindings => AurisPlatform.instance.bindings;

  static String _string(ffi.Pointer<ffi.Char> ptr) =>
      ptr == ffi.nullptr ? '' : ptr.cast<Utf8>().toDartString();
}
