import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:mockingbird_platform_interface/ffi.dart';

/// The FFI platform over the library built at the repository root, for
/// tests on the host; [available] is false when it has not been built.
final class NativeTestPlatform extends MockingbirdFfiPlatform {
  /// Creates the platform.
  factory NativeTestPlatform() = NativeTestPlatform._;

  NativeTestPlatform._();

  /// The library's path under the repository's `build/`.
  static String get path {
    final name = Platform.isMacOS
        ? 'libmockingbird.dylib'
        : Platform.isWindows
        ? 'mockingbird.dll'
        : 'libmockingbird.so';
    return '${Directory.current.path}/../../build/$name';
  }

  /// Whether the library exists.
  static bool get available => File(path).existsSync();

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.open(path);
  ffi.DynamicLibrary? _library;
}
