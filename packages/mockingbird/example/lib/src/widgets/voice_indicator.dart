import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';

/// The language-agnostic "human voice" lamp.
final class VoiceIndicator extends StatelessWidget {
  const factory VoiceIndicator({
    required bool active,
    required bool running,
    Key? key,
  }) = VoiceIndicator._;

  const VoiceIndicator._({
    required this.active,
    required this.running,
    super.key,
  });

  final bool active;
  final bool running;

  @override
  Widget build(BuildContext context) {
    final on = running && active;
    final color = on ? Colors.greenAccent : Colors.grey;
    return Card(
      color: on ? Colors.green.withValues(alpha: 0.18) : null,
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 12),
        child: Row(
          children: [
            Icon(
              on ? Icons.record_voice_over : Icons.voice_over_off,
              color: color,
              size: 32,
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    on ? 'HUMAN VOICE' : 'No voice',
                    style: TextStyle(
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                      color: color,
                    ),
                  ),
                  const Text(
                    kIsWeb
                        ? 'Fast VAD (libfvad), any language'
                        : 'Multilingual (Silero VAD), any language',
                    style: TextStyle(fontSize: 12, color: Colors.grey),
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
