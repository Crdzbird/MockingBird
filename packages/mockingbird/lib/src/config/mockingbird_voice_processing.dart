import 'package:flutter/foundation.dart' show immutable;

/// The platform's own voice processing for live capture: echo cancellation,
/// automatic gain and noise suppression. Each platform applies what it has;
/// `MockingbirdStats.capture` reports what is in effect.
@immutable
final class MockingbirdVoiceProcessing {
  /// Chooses each stage; all off captures the raw microphone.
  const factory MockingbirdVoiceProcessing({
    bool echoCancellation,
    bool gainControl,
    bool noiseSuppression,
  }) = MockingbirdVoiceProcessing._;

  const MockingbirdVoiceProcessing._({
    this.echoCancellation = false,
    this.gainControl = false,
    this.noiseSuppression = false,
  });

  /// Every stage on, as voice and video calls use.
  const factory MockingbirdVoiceProcessing.all() =
      MockingbirdVoiceProcessing._all;

  const MockingbirdVoiceProcessing._all()
    : this._(echoCancellation: true, gainControl: true, noiseSuppression: true);

  /// Reads a `MOCKINGBIRD_VOICE_*` mask.
  factory MockingbirdVoiceProcessing.fromNative(int mask) =>
      MockingbirdVoiceProcessing(
        echoCancellation: mask & 1 != 0,
        gainControl: mask & 2 != 0,
        noiseSuppression: mask & 4 != 0,
      );

  /// Removes the device's own playback from the microphone signal.
  final bool echoCancellation;

  /// Levels the input automatically.
  final bool gainControl;

  /// The platform's noise suppressor, separate from RNNoise.
  final bool noiseSuppression;

  /// The `MOCKINGBIRD_VOICE_*` mask.
  int get nativeValue =>
      (echoCancellation ? 1 : 0) |
      (gainControl ? 2 : 0) |
      (noiseSuppression ? 4 : 0);

  @override
  bool operator ==(Object other) =>
      other is MockingbirdVoiceProcessing && other.nativeValue == nativeValue;

  @override
  int get hashCode => nativeValue;
}
