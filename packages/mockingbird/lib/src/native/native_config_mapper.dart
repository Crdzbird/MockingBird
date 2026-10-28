import 'package:mockingbird/src/config/mockingbird_config.dart';
import 'package:mockingbird/src/native/native_detector_mapper.dart';
import 'package:mockingbird/src/native/native_output_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Flattens a [MockingbirdConfig] into the platform interface's mirror.
final class NativeConfigMapper {
  /// Creates the mapper.
  const factory NativeConfigMapper() = NativeConfigMapper._;

  const NativeConfigMapper._();

  /// The mirror of [config].
  MockingbirdNativeConfig map(MockingbirdConfig config) =>
      MockingbirdNativeConfig(
        backend: config.backend.nativeValue,
        sampleRateHz: config.sampleRateHz,
        frameMs: config.frameMs,
        enableRnnoise: config.denoise,
        detectors: const NativeDetectorMapper().map(config),
        output: const NativeOutputMapper().map(config),
      );
}
