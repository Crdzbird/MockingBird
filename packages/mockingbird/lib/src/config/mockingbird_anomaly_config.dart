import 'package:mockingbird/src/config/sensitivity.dart';

/// The anomaly gate: sudden or unusual sounds against a rolling background.
final class MockingbirdAnomalyConfig {
  /// Creates an anomaly configuration.
  const factory MockingbirdAnomalyConfig({
    bool enabled,
    double rmsZ,
    double fluxZ,
    int backgroundMs,
  }) = MockingbirdAnomalyConfig._;

  const MockingbirdAnomalyConfig._({
    this.enabled = true,
    this.rmsZ = 3.0,
    this.fluxZ = 3.0,
    this.backgroundMs = 2000,
  });

  /// Derives both z-score triggers from a [Sensitivity] dial: 5.0 to 1.5.
  factory MockingbirdAnomalyConfig.sensitive(
    Sensitivity s, {
    bool enabled = true,
  }) => MockingbirdAnomalyConfig(
    enabled: enabled,
    rmsZ: s.lerp(5, 1.5),
    fluxZ: s.lerp(5, 1.5),
  );

  /// Whether the anomaly gate runs.
  final bool enabled;

  /// RMS z-score trigger; lower is more sensitive.
  final double rmsZ;

  /// Spectral-flux z-score trigger; lower is more sensitive.
  final double fluxZ;

  /// Rolling background window the z-scores are measured against, ms.
  final int backgroundMs;
}
