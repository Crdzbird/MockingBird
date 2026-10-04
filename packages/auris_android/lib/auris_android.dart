/// Android implementation of AurisPlatform.
///
/// libauris.so is shipped inside the AAR (built by gradle's
/// externalNativeBuild → CMakeLists.txt at android/src/main/cpp). At runtime
/// we open it from the app's native library path via DynamicLibrary.open.
library;

import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/auris_platform_interface.dart';

/// {@template auris_android}
/// Android implementation of [AurisPlatform].
/// {@endtemplate}
class AurisAndroid extends AurisPlatform {
  /// {@macro auris_android}
  AurisAndroid();

  /// Registers this class as the default instance of [AurisPlatform].
  ///
  /// Called automatically by Flutter's plugin registry on Android.
  static void registerWith() {
    AurisPlatform.instance = AurisAndroid();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.open('libauris.so');
}
