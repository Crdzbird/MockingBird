// Public surface of auris_platform_interface.
//
// Each platform implementation (auris_ios, auris_macos, auris_android) extends
// [AurisPlatform] and supplies the [DynamicLibrary] that exposes libauris.
// Higher-level code in the `auris` package reaches the C ABI through
// `AurisPlatform.instance.bindings`.

import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/src/bindings.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

export 'src/bindings.dart';

/// {@template auris_platform}
/// Federated interface every Auris platform implementation must satisfy.
///
/// Platform implementations should `extend` (not `implement`) this class so
/// that newly added methods do not break existing implementations.
/// {@endtemplate}
abstract class AurisPlatform extends PlatformInterface {
  /// {@macro auris_platform}
  AurisPlatform() : super(token: _token);

  static final Object _token = Object();

  static AurisPlatform? _instance;

  /// The currently registered concrete implementation.
  ///
  /// Throws [StateError] when no implementation has registered itself yet,
  /// which means the caller is running on a platform that has not been
  /// wired up (e.g. unit tests without a fake installed).
  static AurisPlatform get instance {
    final i = _instance;
    if (i == null) {
      throw StateError(
        'No AurisPlatform implementation has been registered. The platform '
        'plugin must call `AurisPlatform.instance = ...` during startup.',
      );
    }
    return i;
  }

  /// Register the concrete platform implementation.
  static set instance(AurisPlatform value) {
    PlatformInterface.verify(value, _token);
    _instance = value;
  }

  /// The dynamically-loaded native library exposing libauris.
  ///
  /// Implementations cache the lookup result. On iOS / macOS the symbols
  /// live in the host process (`DynamicLibrary.process()`). On Android the
  /// `.so` is opened via `DynamicLibrary.open('libauris.so')`.
  ffi.DynamicLibrary get nativeLibrary;

  /// Generated bindings against [nativeLibrary]. Lazily constructed and
  /// memoised on first access.
  AurisBindings get bindings => _bindings ??= AurisBindings(nativeLibrary);
  AurisBindings? _bindings;
}
