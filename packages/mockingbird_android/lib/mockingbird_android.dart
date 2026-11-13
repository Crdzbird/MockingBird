/// The android implementation of the mockingbird plugin.
library;

import 'dart:ffi' as ffi;

import 'package:flutter/services.dart';
import 'package:mockingbird_platform_interface/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Registers the FFI platform over the library this platform links, and
/// audio focus for the player over the plugin's channel.
final class MockingbirdAndroid extends MockingbirdFfiPlatform {
  /// Creates the platform; [registerWith] installs it.
  factory MockingbirdAndroid() = MockingbirdAndroid._;

  MockingbirdAndroid._();

  /// Called by the Flutter plugin registrant.
  static void registerWith() {
    MockingbirdPlatform.instance = MockingbirdAndroid();
  }

  static const _methods = MethodChannel('io.mockingbird/playback');
  static const _events = EventChannel('io.mockingbird/playback_focus');

  ffi.DynamicLibrary? _library;

  @override
  ffi.DynamicLibrary get nativeLibrary =>
      _library ??= ffi.DynamicLibrary.open('libmockingbird.so');

  @override
  Future<void> requestPlaybackFocus() async {
    await _methods.invokeMethod<bool>('requestFocus');
  }

  @override
  Future<void> abandonPlaybackFocus() async {
    await _methods.invokeMethod<void>('abandonFocus');
  }

  @override
  Stream<MockingbirdPlaybackFocus> get playbackFocusChanges => _events
      .receiveBroadcastStream()
      .map((name) => MockingbirdPlaybackFocus.values.byName(name! as String));
}
