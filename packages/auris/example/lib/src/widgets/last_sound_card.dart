import 'package:auris/auris.dart';
import 'package:auris_example/src/demo/detection_summary.dart';
import 'package:flutter/material.dart';

/// The latest recognised sound, colour-coded by its broad category.
class LastSoundCard extends StatelessWidget {
  const LastSoundCard({
    required this.detection,
    required this.running,
    super.key,
  });

  final DetectionSummary detection;
  final bool running;

  Color get _color => switch (detection.lastCategory) {
    AurisSoundCategory.speech => Colors.lightBlueAccent,
    AurisSoundCategory.humanOther => Colors.cyanAccent,
    AurisSoundCategory.animal => Colors.orangeAccent,
    AurisSoundCategory.music => Colors.purpleAccent,
    AurisSoundCategory.nature => Colors.tealAccent,
    AurisSoundCategory.other => Colors.grey,
  };

  @override
  Widget build(BuildContext context) {
    final confidence = (detection.lastConfidence * 100).round();
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          children: [
            Container(
              width: 10,
              height: 36,
              decoration: BoxDecoration(
                color: running ? _color : Colors.grey,
                borderRadius: BorderRadius.circular(4),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    running ? detection.lastSound : '—',
                    style: const TextStyle(
                      fontSize: 18,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  Text(
                    running
                        ? '${detection.lastCategory.name} · $confidence%'
                        : 'Latest recognized sound',
                    style: const TextStyle(fontSize: 12, color: Colors.grey),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
