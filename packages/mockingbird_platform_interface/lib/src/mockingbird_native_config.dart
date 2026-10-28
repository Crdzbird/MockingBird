import 'package:mockingbird_platform_interface/src/mockingbird_native_detector_config.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_output_config.dart';

/// A mirror of `mockingbird_config_t`, grouped so each part stays small.
final class MockingbirdNativeConfig {
  /// Creates a config; every field is set explicitly by the caller.
  const factory MockingbirdNativeConfig({
    required int backend,
    required int sampleRateHz,
    required int frameMs,
    required bool enableRnnoise,
    required int voiceProcessing,
    required MockingbirdNativeDetectorConfig detectors,
    required MockingbirdNativeOutputConfig output,
  }) = MockingbirdNativeConfig._;

  const MockingbirdNativeConfig._({
    required this.backend,
    required this.sampleRateHz,
    required this.frameMs,
    required this.enableRnnoise,
    required this.voiceProcessing,
    required this.detectors,
    required this.output,
  });

  /// `mockingbird_backend_t` as an integer.
  final int backend;

  /// Sample rate in hertz.
  final int sampleRateHz;

  /// Frame length in milliseconds.
  final int frameMs;

  /// Whether RNNoise runs before the detectors.
  final bool enableRnnoise;

  /// Mirrors `voice_processing`, a `MOCKINGBIRD_VOICE_*` mask.
  final int voiceProcessing;

  /// The detector fields.
  final MockingbirdNativeDetectorConfig detectors;

  /// The segment, encoder, queue and performance fields.
  final MockingbirdNativeOutputConfig output;
}
