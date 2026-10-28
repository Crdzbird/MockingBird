import 'package:flutter/material.dart';

/// The single dial that tunes every detector of the active preset.
final class SensitivitySlider extends StatelessWidget {
  const factory SensitivitySlider({
    required double value,
    required ValueChanged<double> onChanged,
    required ValueChanged<double> onChangeEnd,
    Key? key,
  }) = SensitivitySlider._;

  const SensitivitySlider._({
    required this.value,
    required this.onChanged,
    required this.onChangeEnd,
    super.key,
  });

  final double value;
  final ValueChanged<double> onChanged;
  final ValueChanged<double> onChangeEnd;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      const Icon(Icons.tune, size: 18),
      const SizedBox(width: 8),
      Text('Sensitivity ${(value * 100).round()}%'),
      Expanded(
        child: Slider(
          value: value,
          onChanged: onChanged,
          onChangeEnd: onChangeEnd,
        ),
      ),
    ],
  );
}
