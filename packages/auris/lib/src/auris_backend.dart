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

  /// The backend [auto] resolves to on the running platform.
  static AurisBackend get platformDefault {
    if (kIsWeb) return hostPcm;
    return switch (defaultTargetPlatform) {
      TargetPlatform.iOS || TargetPlatform.macOS => coreAudio,
      TargetPlatform.android => oboe,
      _ => miniaudio,
    };
  }
}
