import 'package:flutter/material.dart';

/// Live input level from the engine's stats, which works whether or not a
/// segment is open.
class LevelMeter extends StatelessWidget {
  const LevelMeter({
    required this.rmsDbfs,
    required this.peakDbfs,
    required this.running,
    super.key,
  });

  final double rmsDbfs;
  final double peakDbfs;
  final bool running;

  static double _norm(double dbfs) => ((dbfs + 60) / 60).clamp(0.0, 1.0);

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final level = running ? _norm(rmsDbfs) : 0.0;
    final peak = running ? _norm(peakDbfs) : 0.0;
    final hot = peakDbfs > -3;
    return Row(
      children: [
        Icon(Icons.graphic_eq, size: 18, color: cs.onSurfaceVariant),
        const SizedBox(width: 8),
        Expanded(
          child: Stack(
            alignment: Alignment.centerLeft,
            children: [
              ClipRRect(
                borderRadius: BorderRadius.circular(4),
                child: LinearProgressIndicator(
                  value: level,
                  minHeight: 10,
                  backgroundColor: cs.surfaceContainerHighest,
                  color: hot ? cs.error : cs.primary,
                ),
              ),
              if (running)
                FractionallySizedBox(
                  widthFactor: peak,
                  child: Align(
                    alignment: Alignment.centerRight,
                    child: Container(width: 2, height: 14, color: cs.onSurface),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(width: 8),
        SizedBox(
          width: 64,
          child: Text(
            running ? '${rmsDbfs.toStringAsFixed(1)} dB' : '—',
            style: Theme.of(context).textTheme.labelSmall,
            textAlign: TextAlign.right,
          ),
        ),
      ],
    );
  }
}
