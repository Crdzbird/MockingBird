import 'dart:io' show Platform;

/// Where the engine takes audio from.
enum AurisBackend {
  /// Let the engine pick the best native backend for the platform.
  auto(0),

  /// Cross-platform miniaudio capture.
  miniaudio(1),

  /// Google Oboe: low-latency Android capture.
  oboe(2),

  /// CoreAudio with hardware timestamps (iOS, macOS).
  coreAudio(3),

  /// No microphone: the app feeds PCM through `AurisEngine.pushPcm`.
  hostPcm(4);

  const AurisBackend(this.nativeValue);

  /// The `auris_backend_t` value.
  final int nativeValue;

  /// The native microphone backend for the platform this code runs on.
  static AurisBackend get platformDefault => switch (Platform.operatingSystem) {
    'ios' || 'macos' => coreAudio,
    'android' => oboe,
    _ => miniaudio,
  };
}
