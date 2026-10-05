import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:mockingbird/mockingbird.dart';

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

  /// Whether the preset still detects something on the running platform.
  /// The web build has no neural detectors, so a classifier-only preset
  /// would do nothing there.
  bool get availableHere => !kIsWeb || vad || anomaly;

  /// The engine configuration for this preset on the live microphone.
  /// On the web the neural tier falls back to the fast one and the sound
  /// classifier stays off.
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
    vadTier: neural && !kIsWeb ? .neural : .fast,
    neuralVadModelPath: kIsWeb ? null : neuralVadModelPath,
    anomaly: anomaly,
    soundClass: soundClass && !kIsWeb,
    soundClassModelPath: kIsWeb ? null : soundClassModelPath,
    performance: const AurisPerformanceConfig.mobile(),
  );
}
