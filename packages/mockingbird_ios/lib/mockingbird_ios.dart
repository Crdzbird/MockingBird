/// The ios implementation of the mockingbird plugin.
library;

import 'dart:ffi' as ffi;

import 'package:mockingbird_platform_interface/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Registers the FFI platform over the library this platform links.
final class MockingbirdIOS extends MockingbirdFfiPlatform {
  /// Creates the platform; [registerWith] installs it.
  factory MockingbirdIOS() = MockingbirdIOS._;

  MockingbirdIOS._();

  /// Called by the Flutter plugin registrant.
  static void registerWith() {
    MockingbirdPlatform.instance = MockingbirdIOS();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.process();
}
