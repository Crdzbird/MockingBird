import 'package:flutter/material.dart';
import 'package:mockingbird/mockingbird.dart';

/// A named detector recipe. The same engine does very different jobs
/// depending on which detectors a preset switches on.
final class DemoPreset {
  const factory DemoPreset(
    String label,
    IconData icon, {
    double sensitivity,
    bool neural,
    bool vad,
    bool anomaly,
    bool soundClass,
    bool denoise,
  }) = DemoPreset._;

  const DemoPreset._(
    this.label,
    this.icon, {
    this.sensitivity = 0.6,
    this.neural = true,
    this.vad = true,
    this.anomaly = true,
    this.soundClass = true,
    this.denoise = false,
  });

  final String label;
  final IconData icon;
  final double sensitivity;
  final bool neural;
  final bool vad;
  final bool anomaly;
  final bool soundClass;
  final bool denoise;

  /// The engine configuration for this preset on the live microphone.
  MockingbirdConfig config({
    required double sensitivity,
    required bool denoise,
    required String neuralVadModelPath,
    required String soundClassModelPath,
  }) => MockingbirdConfig.sensitive(
    sensitivity,
    backend: MockingbirdBackend.platformDefault,
    denoise: denoise || this.denoise,
    vad: vad,
    vadTier: neural ? .neural : .fast,
    neuralVadModelPath: neuralVadModelPath,
    anomaly: anomaly,
    soundClass: soundClass,
    soundClassModelPath: soundClassModelPath,
    performance: const MockingbirdPerformanceConfig.mobile(),
  );
}
