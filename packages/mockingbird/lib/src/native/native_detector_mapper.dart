import 'package:mockingbird/src/config/mockingbird_config.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Flattens the detector groups of a [MockingbirdConfig].
final class NativeDetectorMapper {
  /// Creates the mapper.
  const factory NativeDetectorMapper() = NativeDetectorMapper._;

  const NativeDetectorMapper._();

  /// The detector mirror of [c].
  MockingbirdNativeDetectorConfig map(MockingbirdConfig c) =>
      MockingbirdNativeDetectorConfig(
        enableVad: c.vad.enabled,
        vadTier: c.vad.tier.nativeValue,
        vadAggressiveness: c.vad.aggressiveness,
        vadNeuralThreshold: c.vad.neuralThreshold,
        vadHangoverMs: c.vad.hangoverMs,
        neuralVadModelPath: c.vad.modelPath,
        enableAnomaly: c.anomaly.enabled,
        anomalyRmsZ: c.anomaly.rmsZ,
        anomalyFluxZ: c.anomaly.fluxZ,
        anomalyBackgroundMs: c.anomaly.backgroundMs,
        enableSoundClass: c.soundClass.enabled,
        soundClassModelPath: c.soundClass.modelPath,
        soundClassThreshold: c.soundClass.threshold,
        soundClassIntervalMs: c.soundClass.intervalMs,
      );
}
