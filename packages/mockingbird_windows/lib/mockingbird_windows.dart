/// The windows implementation of the mockingbird plugin.
library;

import 'dart:ffi' as ffi;

import 'package:mockingbird_platform_interface/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Registers the FFI platform over the library this plugin bundles.
final class MockingbirdWindows extends MockingbirdFfiPlatform {
  /// Creates the platform; [registerWith] installs it.
  factory MockingbirdWindows() = MockingbirdWindows._;

  MockingbirdWindows._();

  /// Called by the Flutter plugin registrant.
  static void registerWith() {
    MockingbirdPlatform.instance = MockingbirdWindows();
  }

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.open('mockingbird.dll');
}
