import 'package:mockingbird/src/config/mockingbird_voice_processing.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// What the capture backend is doing for the microphone.
final class MockingbirdCaptureHealth {
  /// Creates a snapshot.
  const factory MockingbirdCaptureHealth({
    required MockingbirdVoiceProcessing voiceProcessing,
    required int restarts,
  }) = MockingbirdCaptureHealth._;

  const MockingbirdCaptureHealth._({
    required this.voiceProcessing,
    required this.restarts,
  });

  /// Reads the capture fields of a native `mockingbird_stats_t`.
  factory MockingbirdCaptureHealth.fromNative(MockingbirdNativeStats s) =>
      MockingbirdCaptureHealth(
        voiceProcessing: .fromNative(s.voiceProcessingActive),
        restarts: s.captureRestarts,
      );

  /// The voice processing the platform applied, which can be less than
  /// asked for.
  final MockingbirdVoiceProcessing voiceProcessing;

  /// Times capture came back after an interruption or a route change.
  final int restarts;
}
