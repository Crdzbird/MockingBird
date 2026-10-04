import 'package:auris_platform_interface/src/auris_native_detector_config.dart';
import 'package:auris_platform_interface/src/auris_native_output_config.dart';

/// A mirror of `auris_config_t`, grouped so each part stays small.
final class AurisNativeConfig {
  /// Creates a config; every field is set explicitly by the caller.
  const AurisNativeConfig({
    required this.backend,
    required this.sampleRateHz,
    required this.frameMs,
    required this.enableRnnoise,
    required this.detectors,
    required this.output,
  });

  /// `auris_backend_t` as an integer.
  final int backend;

  /// Sample rate in hertz.
  final int sampleRateHz;

  /// Frame length in milliseconds.
  final int frameMs;

  /// Whether RNNoise runs before the detectors.
  final bool enableRnnoise;

  /// The detector fields.
  final AurisNativeDetectorConfig detectors;

  /// The segment, encoder, queue and performance fields.
  final AurisNativeOutputConfig output;
}
