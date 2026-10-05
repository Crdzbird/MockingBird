import 'package:flutter/material.dart';

/// Newest-first console of formatted events.
class EventLog extends StatelessWidget {
  const EventLog({required this.lines, super.key});

  final List<String> lines;

  @override
  Widget build(BuildContext context) => Card(
    child: lines.isEmpty
        ? const Center(child: Text('No events yet.'))
        : ListView.builder(
            padding: const EdgeInsets.all(8),
            itemCount: lines.length,
            itemBuilder: (context, i) => Text(
              lines[i],
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
            ),
          ),
  );
}
