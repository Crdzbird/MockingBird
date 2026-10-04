/// macOS implementation of AurisPlatform.
///
/// libauris is compiled into the host app statically via the podspec, so the
/// C symbols live in the running process. We reach them with
/// `DynamicLibrary.process()` rather than `DynamicLibrary.open(...)`.
library;

import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/auris_platform_interface.dart';

/// {@template auris_macos}
/// The macOS implementation of [AurisPlatform].
/// {@endtemplate}
class AurisMacOS extends AurisPlatform {
  /// {@macro auris_macos}
  AurisMacOS();

  /// Registers this class as the default instance of [AurisPlatform].
  ///
  /// Called automatically by Flutter's plugin registry on macOS.
  static void registerWith() {
    AurisPlatform.instance = AurisMacOS();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.process();
}
