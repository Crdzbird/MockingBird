/// The ios implementation of the auris plugin.
library;

import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:auris_platform_interface/ffi.dart';

/// Registers the FFI platform over the library this platform links.
class AurisIOS extends AurisFfiPlatform {
  /// Creates the platform; [registerWith] installs it.
  AurisIOS();

  /// Called by the Flutter plugin registrant.
  static void registerWith() {
    AurisPlatform.instance = AurisIOS();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary => _library ??= ffi.DynamicLibrary.process();
}
