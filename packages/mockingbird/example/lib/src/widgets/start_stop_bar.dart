import 'package:flutter/material.dart';

/// Start and Stop, one of which is always enabled.
final class StartStopBar extends StatelessWidget {
  const factory StartStopBar({
    required bool running,
    required VoidCallback onStart,
    required VoidCallback onStop,
    Key? key,
  }) = StartStopBar._;

  const StartStopBar._({
    required this.running,
    required this.onStart,
    required this.onStop,
    super.key,
  });

  final bool running;
  final VoidCallback onStart;
  final VoidCallback onStop;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Expanded(
        child: FilledButton.icon(
          onPressed: running ? null : onStart,
          icon: const Icon(Icons.mic),
          label: const Text('Start'),
        ),
      ),
      const SizedBox(width: 12),
      Expanded(
        child: OutlinedButton.icon(
          onPressed: running ? onStop : null,
          icon: const Icon(Icons.stop),
          label: const Text('Stop'),
        ),
      ),
    ],
  );
}
