import 'package:auris/src/config/auris_config.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';

/// Flattens the detector groups of an [AurisConfig].
final class NativeDetectorMapper {
  const NativeDetectorMapper();

  /// The detector mirror of [c].
  AurisNativeDetectorConfig map(AurisConfig c) => AurisNativeDetectorConfig(
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
