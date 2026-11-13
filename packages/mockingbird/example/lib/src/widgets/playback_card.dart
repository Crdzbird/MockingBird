import 'package:flutter/material.dart';
import 'package:mockingbird_example/src/demo/playback_state.dart';
import 'package:mockingbird_example/src/widgets/stat_row.dart';

/// Plays back the last capture through the player, with its counters.
final class PlaybackCard extends StatelessWidget {
  const factory PlaybackCard({
    required PlaybackState state,
    required bool capturing,
    required VoidCallback onPlay,
    Key? key,
  }) = PlaybackCard._;

  const PlaybackCard._({
    required this.state,
    required this.capturing,
    required this.onPlay,
    super.key,
  });

  final PlaybackState state;
  final bool capturing;
  final VoidCallback onPlay;

  @override
  Widget build(BuildContext context) {
    final s = state.stats;
    final ready = !capturing && !state.playing && state.kept > Duration.zero;
    final seconds = (state.kept.inMilliseconds / 1000).toStringAsFixed(1);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Playback', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 8),
            Text('Last capture kept: $seconds s of Opus packets'),
            const SizedBox(height: 8),
            FilledButton.tonalIcon(
              key: const Key('play_back'),
              onPressed: ready ? onPlay : null,
              icon: const Icon(Icons.play_arrow),
              label: Text(state.playing ? 'Playing' : 'Play back'),
            ),
            if (state.error case final message?) Text(message),
            if (s != null) ...[
              const SizedBox(height: 8),
              Text('state: ${s.state.name}'),
              StatRow('packets_played', s.played),
              StatRow('packets_concealed', s.concealed),
              StatRow('packets_dropped', s.dropped),
              StatRow('underruns', s.underruns),
              StatRow('buffered_ms', s.buffered.inMilliseconds),
            ],
          ],
        ),
      ),
    );
  }
}
