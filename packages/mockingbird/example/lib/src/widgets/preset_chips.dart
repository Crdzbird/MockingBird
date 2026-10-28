import 'package:flutter/material.dart';
import 'package:mockingbird_example/src/demo/demo_presets.dart';

/// One chip per preset; tapping recreates the engine with that recipe. A
/// preset the platform cannot run is shown disabled.
final class PresetChips extends StatelessWidget {
  const factory PresetChips({
    required int selected,
    required ValueChanged<int> onSelected,
    Key? key,
  }) = PresetChips._;

  const PresetChips._({
    required this.selected,
    required this.onSelected,
    super.key,
  });

  final int selected;
  final ValueChanged<int> onSelected;

  @override
  Widget build(BuildContext context) => Wrap(
    spacing: 8,
    runSpacing: 4,
    children: [
      for (final (i, preset) in demoPresets.indexed)
        ChoiceChip(
          avatar: Icon(preset.icon, size: 18),
          label: Text(preset.label),
          selected: selected == i,
          onSelected: preset.availableHere ? (_) => onSelected(i) : null,
        ),
    ],
  );
}
