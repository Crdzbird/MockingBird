import 'package:flutter/material.dart';

/// One name/value line with tabular digits so columns do not jitter.
final class StatRow extends StatelessWidget {
  const factory StatRow(String name, int value, {Key? key}) = StatRow._;

  const StatRow._(this.name, this.value, {super.key});

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
