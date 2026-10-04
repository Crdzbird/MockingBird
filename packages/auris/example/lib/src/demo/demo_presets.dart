import 'package:auris_example/src/demo/demo_preset.dart';
import 'package:flutter/material.dart';

/// The recipes offered by the demo.
const demoPresets = <DemoPreset>[
  DemoPreset('Balanced', Icons.tune),
  DemoPreset(
    'Voice only',
    Icons.record_voice_over,
    anomaly: false,
    soundClass: false,
    denoise: true,
  ),
  DemoPreset('Everything', Icons.hearing, sensitivity: 0.75),
  DemoPreset(
    'Anomaly watch',
    Icons.warning_amber_rounded,
    vad: false,
    soundClass: false,
    sensitivity: 0.8,
  ),
  DemoPreset('Sound ID', Icons.graphic_eq, vad: false, anomaly: false),
];
