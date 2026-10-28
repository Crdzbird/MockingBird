/// The detector fields of `mockingbird_config_t`.
final class MockingbirdNativeDetectorConfig {
  /// Creates the detector fields.
  const factory MockingbirdNativeDetectorConfig({
    required bool enableVad,
    required int vadTier,
    required int vadAggressiveness,
    required double vadNeuralThreshold,
    required int vadHangoverMs,
    required String? neuralVadModelPath,
    required bool enableAnomaly,
    required double anomalyRmsZ,
    required double anomalyFluxZ,
    required int anomalyBackgroundMs,
    required bool enableSoundClass,
    required String? soundClassModelPath,
    required double soundClassThreshold,
    required int soundClassIntervalMs,
  }) = MockingbirdNativeDetectorConfig._;

  const MockingbirdNativeDetectorConfig._({
    required this.enableVad,
    required this.vadTier,
    required this.vadAggressiveness,
    required this.vadNeuralThreshold,
    required this.vadHangoverMs,
    required this.neuralVadModelPath,
    required this.enableAnomaly,
    required this.anomalyRmsZ,
    required this.anomalyFluxZ,
    required this.anomalyBackgroundMs,
    required this.enableSoundClass,
    required this.soundClassModelPath,
    required this.soundClassThreshold,
    required this.soundClassIntervalMs,
  });

  /// Mirrors `enable_vad`.
  final bool enableVad;

  /// `mockingbird_vad_tier_t` as an integer.
  final int vadTier;

  /// Mirrors `vad_aggressiveness`.
  final int vadAggressiveness;

  /// Mirrors `vad_neural_threshold`.
  final double vadNeuralThreshold;

  /// Mirrors `vad_hangover_ms`.
  final int vadHangoverMs;

  /// Mirrors `neural_vad_model_path`; null for the engine default.
  final String? neuralVadModelPath;

  /// Mirrors `enable_anomaly`.
  final bool enableAnomaly;

  /// Mirrors `anomaly_rms_z`.
  final double anomalyRmsZ;

  /// Mirrors `anomaly_flux_z`.
  final double anomalyFluxZ;

  /// Mirrors `anomaly_background_ms`.
  final int anomalyBackgroundMs;

  /// Mirrors `enable_sound_class`.
  final bool enableSoundClass;

  /// Mirrors `sound_class_model_path`; null for the engine default.
  final String? soundClassModelPath;

  /// Mirrors `sound_class_threshold`.
  final double soundClassThreshold;

  /// Mirrors `sound_class_interval_ms`.
  final int soundClassIntervalMs;
}
