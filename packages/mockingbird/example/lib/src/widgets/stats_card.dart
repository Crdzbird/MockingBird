import 'package:flutter/material.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/widgets/stat_row.dart';

/// Engine counters, worker health and what the host actually received.
class StatsCard extends StatelessWidget {
  const StatsCard({
    required this.stats,
    required this.running,
    required this.packetsReceived,
    required this.bytesReceived,
    super.key,
  });

  final AurisStats? stats;
  final bool running;
  final int packetsReceived;
  final int bytesReceived;

  @override
  Widget build(BuildContext context) {
    final s = stats;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  running
                      ? Icons.fiber_manual_record
                      : Icons.fiber_manual_record_outlined,
                  color: running ? Colors.redAccent : Colors.grey,
                  size: 12,
                ),
                const SizedBox(width: 8),
                Text(
                  running ? 'Capturing' : 'Idle',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ],
            ),
            const SizedBox(height: 12),
            if (s == null)
              const Text('No stats yet. Press Start.')
            else ...[
              StatRow('frames_captured', s.framesCaptured),
              StatRow('frames_dropped_capture', s.framesDroppedCapture),
              StatRow('packets_emitted', s.packetsEmitted),
              StatRow('packets_dropped', s.packetsDropped),
              StatRow('packets_received (host)', packetsReceived),
              StatRow('bytes_received (host)', bytesReceived),
              StatRow('segments_opened', s.segmentsOpened),
              StatRow('segments_closed', s.segmentsClosed),
              StatRow('pcm_ring_fill_frames', s.pcmRingFillFrames),
              StatRow('packet_queue_fill', s.packetQueueFill),
              const Divider(height: 16),
              StatRow('worker_busy_%', (s.worker.busyFraction * 100).round()),
              StatRow('worker_max_frame_us', s.worker.maxFrameUs),
              StatRow('worker_wakeups', s.worker.wakeups),
              StatRow('yamnet_inferences', s.worker.soundClassInferences),
              StatRow('yamnet_skipped', s.worker.soundClassInferencesSkipped),
              StatRow('rnnoise_skipped_quiet', s.worker.preprocFramesSkipped),
              StatRow('degradation_level', s.worker.degradationLevel),
            ],
          ],
        ),
      ),
    );
  }
}
