import 'package:mockingbird/src/config/auris_config.dart';
import 'package:mockingbird/src/native/native_detector_mapper.dart';
import 'package:mockingbird/src/native/native_output_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Flattens an [AurisConfig] into the platform interface's mirror.
final class NativeConfigMapper {
  const NativeConfigMapper();

  /// The mirror of [config].
  AurisNativeConfig map(AurisConfig config) => AurisNativeConfig(
    backend: config.backend.nativeValue,
    sampleRateHz: config.sampleRateHz,
    frameMs: config.frameMs,
    enableRnnoise: config.denoise,
    detectors: const NativeDetectorMapper().map(config),
    output: const NativeOutputMapper().map(config),
  );
}
