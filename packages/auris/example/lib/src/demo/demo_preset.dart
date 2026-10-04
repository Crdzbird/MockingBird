import 'package:auris/auris.dart';
import 'package:flutter/material.dart';

/// A named detector recipe. The same engine does very different jobs
/// depending on which detectors a preset switches on.
final class DemoPreset {
  const DemoPreset(
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
  AurisConfig config({
    required double sensitivity,
    required bool denoise,
    required String neuralVadModelPath,
    required String soundClassModelPath,
  }) => AurisConfig.sensitive(
    sensitivity,
    backend: AurisBackend.platformDefault,
    denoise: denoise || this.denoise,
    vad: vad,
    vadTier: neural ? .neural : .fast,
    neuralVadModelPath: neuralVadModelPath,
    anomaly: anomaly,
    soundClass: soundClass,
    soundClassModelPath: soundClassModelPath,
    performance: const AurisPerformanceConfig.mobile(),
  );
}
