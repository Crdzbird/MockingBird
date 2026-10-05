/// The macos implementation of the auris plugin.
library;

import 'dart:ffi' as ffi;

import 'package:mockingbird_platform_interface/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Registers the FFI platform over the library this platform links.
class AurisMacOS extends AurisFfiPlatform {
  /// Creates the platform; [registerWith] installs it.
  AurisMacOS();

  /// Called by the Flutter plugin registrant.
  static void registerWith() {
    AurisPlatform.instance = AurisMacOS();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.process();
}
