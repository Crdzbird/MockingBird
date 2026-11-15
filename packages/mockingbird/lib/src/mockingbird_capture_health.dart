import 'package:mockingbird/src/config/mockingbird_voice_processing.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// What the capture backend is doing for the microphone.
final class MockingbirdCaptureHealth {
  /// Creates a snapshot.
  const factory MockingbirdCaptureHealth({
    required MockingbirdVoiceProcessing voiceProcessing,
    required int restarts,
    int gaps,
    Duration gapTime,
  }) = MockingbirdCaptureHealth._;

  const MockingbirdCaptureHealth._({
    required this.voiceProcessing,
    required this.restarts,
    this.gaps = 0,
    this.gapTime = Duration.zero,
  });

  /// Reads the capture fields of a native `mockingbird_stats_t`.
  factory MockingbirdCaptureHealth.fromNative(MockingbirdNativeStats s) =>
      MockingbirdCaptureHealth(
        voiceProcessing: .fromNative(s.voiceProcessingActive),
        restarts: s.captureRestarts,
        gaps: s.captureGaps,
        gapTime: Duration(milliseconds: s.captureGapMs),
      );

  /// The voice processing the platform applied, which can be less than
  /// asked for.
  final MockingbirdVoiceProcessing voiceProcessing;

  /// Times capture came back after an interruption or a route change.
  final int restarts;

  /// Times the platform delivered no audio for more than 200 ms beyond what
  /// it had delivered: a stalled audio DSP, a late callback, an interruption.
  /// Always 0 for host PCM and on the web.
  final int gaps;

  /// The audio those [gaps] lost, in total.
  final Duration gapTime;
}
