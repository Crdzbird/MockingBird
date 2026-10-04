import 'package:flutter/foundation.dart' show TargetPlatform, defaultTargetPlatform, kIsWeb;

/// Where audio comes from.
enum AurisBackend {
  /// The best capture backend for the platform.
  auto(0),

  /// miniaudio, the portable backend.
  miniaudio(1),

  /// Oboe on Android.
  oboe(2),

  /// Core Audio on iOS and macOS.
  coreAudio(3),

  /// No capture; the app pushes PCM with `pushPcm`.
  hostPcm(4);

  const AurisBackend(this.nativeValue);

  /// The `auris_backend_t` value.
  final int nativeValue;

  /// The capture backend for the running platform. On the web the platform
  /// package captures through Web Audio itself, so [auto] is the choice.
  static AurisBackend get platformDefault {
    if (kIsWeb) return auto;
    return switch (defaultTargetPlatform) {
      TargetPlatform.iOS || TargetPlatform.macOS => coreAudio,
      TargetPlatform.android => oboe,
      _ => miniaudio,
    };
  }
}
