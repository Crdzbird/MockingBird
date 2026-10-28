import 'package:flutter/material.dart';

/// RNNoise on or off ahead of every detector.
final class DenoiseSwitch extends StatelessWidget {
  const factory DenoiseSwitch({
    required bool value,
    required ValueChanged<bool> onChanged,
    Key? key,
  }) = DenoiseSwitch._;

  const DenoiseSwitch._({
    required this.value,
    required this.onChanged,
    super.key,
  });

  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => SwitchListTile(
    contentPadding: EdgeInsets.zero,
    dense: true,
    secondary: const Icon(Icons.noise_aware),
    title: const Text('Noise suppression (RNNoise)'),
    subtitle: const Text('Denoise mic before VAD / anomaly / classification'),
    value: value,
    onChanged: onChanged,
  );
}
