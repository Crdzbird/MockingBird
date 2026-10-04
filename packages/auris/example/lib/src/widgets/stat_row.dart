import 'package:flutter/material.dart';

/// One name/value line with tabular digits so columns do not jitter.
class StatRow extends StatelessWidget {
  const StatRow(this.name, this.value, {super.key});

  final String name;
  final int value;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 2),
    child: Row(
      children: [
        Expanded(child: Text(name)),
        Text(
          '$value',
          style: const TextStyle(fontFeatures: [FontFeature.tabularFigures()]),
        ),
      ],
    ),
  );
}
