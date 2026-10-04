/// iOS implementation of AurisPlatform.
///
/// libauris is compiled into the host app statically via the podspec, so the
/// C symbols live in the running process. We reach them with
/// `DynamicLibrary.process()` rather than `DynamicLibrary.open(...)`.
library;

import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/auris_platform_interface.dart';

/// {@template auris_ios}
/// The iOS implementation of [AurisPlatform].
/// {@endtemplate}
class AurisIOS extends AurisPlatform {
  /// {@macro auris_ios}
  AurisIOS();

  /// Registers this class as the default instance of [AurisPlatform].
  ///
  /// Called automatically by Flutter's plugin registry on iOS.
  static void registerWith() {
    AurisPlatform.instance = AurisIOS();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.process();
}
