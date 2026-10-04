import 'package:auris/src/config/sensitivity.dart';

/// The anomaly gate: sudden or unusual sounds against a rolling background.
final class AurisAnomalyConfig {
  /// Creates an anomaly configuration.
  const AurisAnomalyConfig({
    this.enabled = true,
    this.rmsZ = 3.0,
    this.fluxZ = 3.0,
    this.backgroundMs = 2000,
  });

  /// Derives both z-score triggers from a [Sensitivity] dial: 5.0 → 1.5.
  factory AurisAnomalyConfig.sensitive(Sensitivity s, {bool enabled = true}) =>
      AurisAnomalyConfig(
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
