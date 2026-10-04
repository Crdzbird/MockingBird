import 'package:auris_example/src/demo/demo_presets.dart';
import 'package:flutter/material.dart';

/// One chip per preset; tapping recreates the engine with that recipe.
class PresetChips extends StatelessWidget {
  const PresetChips({
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
          onSelected: (_) => onSelected(i),
        ),
    ],
  );
}
