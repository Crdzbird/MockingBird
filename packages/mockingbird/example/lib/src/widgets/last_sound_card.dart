import 'package:flutter/material.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/detection_summary.dart';

/// The latest recognised sound, colour-coded by its broad category.
final class LastSoundCard extends StatelessWidget {
  const factory LastSoundCard({
    required DetectionSummary detection,
    required bool running,
    Key? key,
  }) = LastSoundCard._;

  const LastSoundCard._({
    required this.detection,
    required this.running,
    super.key,
  });

  final DetectionSummary detection;
  final bool running;

  Color get _color => switch (detection.lastCategory) {
    MockingbirdSoundCategory.speech => Colors.lightBlueAccent,
    MockingbirdSoundCategory.humanOther => Colors.cyanAccent,
    MockingbirdSoundCategory.animal => Colors.orangeAccent,
    MockingbirdSoundCategory.music => Colors.purpleAccent,
    MockingbirdSoundCategory.nature => Colors.tealAccent,
    MockingbirdSoundCategory.other => Colors.grey,
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
                    running ? detection.lastSound : '-',
                    style: const TextStyle(
                      fontSize: 18,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  Text(
                    running
                        ? '${detection.lastCategory.name}, $confidence%'
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
