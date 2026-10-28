part of 'mockingbird_config.dart';

/// The configuration [MockingbirdConfig.sensitive] derives from one dial.
final class _SensitiveConfig extends MockingbirdConfig {
  _SensitiveConfig(
    double sensitivity, {
    super.backend,
    super.denoise,
    bool vad = true,
    MockingbirdVadTier vadTier = .neural,
    String? neuralVadModelPath,
    bool anomaly = true,
    bool soundClass = true,
    String? soundClassModelPath,
    super.encoder,
    super.performance,
  }) : super._(
         vad: .sensitive(
           Sensitivity(sensitivity),
           enabled: vad,
           tier: vadTier,
           modelPath: neuralVadModelPath,
         ),
         anomaly: .sensitive(Sensitivity(sensitivity), enabled: anomaly),
         soundClass: .sensitive(
           Sensitivity(sensitivity),
           enabled: soundClass,
           modelPath: soundClassModelPath,
         ),
       );
}
