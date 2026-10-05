import 'package:mockingbird/src/config/sensitivity.dart';
import 'package:mockingbird/src/mockingbird_vad_tier.dart';

/// Voice-activity detection: the language-agnostic `SpeechBegin` /
/// `SpeechEnd` events.
final class MockingbirdVadConfig {
  /// Creates a VAD configuration.
  const MockingbirdVadConfig({
    this.enabled = true,
    this.tier = .fast,
    this.aggressiveness = 2,
    this.neuralThreshold = 0.5,
    this.hangoverMs = 300,
    this.modelPath,
  });

  /// Derives the thresholds from a [Sensitivity] dial: Silero's threshold
  /// runs 0.80 → 0.20 and libfvad's aggressiveness 3 → 0.
  factory MockingbirdVadConfig.sensitive(
    Sensitivity s, {
    bool enabled = true,
    MockingbirdVadTier tier = .neural,
    String? modelPath,
  }) => MockingbirdVadConfig(
    enabled: enabled,
    tier: tier,
    aggressiveness: s.lerp(3, 0).round(),
    neuralThreshold: s.lerp(0.80, 0.20),
    modelPath: modelPath,
  );

  /// Whether voice detection runs.
  final bool enabled;

  /// Which detector runs.
  final MockingbirdVadTier tier;

  /// libfvad aggressiveness 0..3; higher rejects more borderline frames.
  /// Ignored by [MockingbirdVadTier.neural].
  final int aggressiveness;

  /// Silero P(speech) threshold 0..1; lower is more sensitive. Ignored by
  /// [MockingbirdVadTier.fast].
  final double neuralThreshold;

  /// Trailing silence before a speech segment closes, milliseconds.
  final int hangoverMs;

  /// Path to the Silero ONNX model; see `MockingbirdModels.neuralVadPath`.
  final String? modelPath;
}
