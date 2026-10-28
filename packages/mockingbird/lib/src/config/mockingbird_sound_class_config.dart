import 'package:mockingbird/src/config/sensitivity.dart';

/// YAMNet sound classification over all 521 AudioSet classes.
final class MockingbirdSoundClassConfig {
  /// Creates a sound-classification configuration.
  const factory MockingbirdSoundClassConfig({
    bool enabled,
    String? modelPath,
    double threshold,
    int intervalMs,
  }) = MockingbirdSoundClassConfig._;

  const MockingbirdSoundClassConfig._({
    this.enabled = false,
    this.modelPath,
    this.threshold = 0.4,
    this.intervalMs = 1000,
  });

  /// Derives the top-1 threshold from a [Sensitivity] dial: 0.70 to 0.12.
  factory MockingbirdSoundClassConfig.sensitive(
    Sensitivity s, {
    bool enabled = true,
    String? modelPath,
    int intervalMs = 1000,
  }) => MockingbirdSoundClassConfig(
    enabled: enabled,
    modelPath: modelPath,
    threshold: s.lerp(0.70, 0.12),
    intervalMs: intervalMs,
  );

  /// Whether classification runs; emits `SoundClass` events.
  final bool enabled;

  /// Path to the YAMNet ONNX model; see `MockingbirdModels.soundClassPath`.
  final String? modelPath;

  /// Top-1 probability needed to emit an event, 0..1.
  final double threshold;

  /// Inference cadence in milliseconds; the 3 s window overlaps anyway, so
  /// halving this roughly halves the cost.
  final int intervalMs;
}
