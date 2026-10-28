import 'package:mockingbird/src/events/mockingbird_event.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/mockingbird_segment.dart';

/// Groups packets by segment and hands each segment over once it has ended:
/// on its last packet, or after the drain that delivered its end event.
final class SegmentAssembler {
  /// Assembles segments at [sampleRateHz] and passes them to [onSegment].
  factory SegmentAssembler({
    required int sampleRateHz,
    required void Function(MockingbirdSegment) onSegment,
  }) = SegmentAssembler._;

  SegmentAssembler._({required this.sampleRateHz, required this.onSegment});

  /// The engine's sample rate.
  final int sampleRateHz;

  /// Receives each finished segment.
  final void Function(MockingbirdSegment) onSegment;

  final Map<int, List<MockingbirdPacket>> _open = {};
  final Set<int> _ended = {};

  /// Adds [p] to its segment, finishing the segment on its last packet. A
  /// segment already under way when listening began is skipped whole.
  void packet(MockingbirdPacket p) {
    if (!p.isKeyframe && !_open.containsKey(p.segmentId)) return;
    (_open[p.segmentId] ??= []).add(p);
    if (p.isLast) _finish(p.segmentId, complete: true);
  }

  /// Notes segment ends; the segment is handed over by [flush].
  void event(MockingbirdEvent e) {
    if (e is MockingbirdSegmentEndEvent) _ended.add(e.segmentId);
  }

  /// Hands over every segment whose end event arrived. Call it after both
  /// queues are drained: the engine queues a segment's packets before its
  /// end event, so by then every packet is here.
  void flush() {
    final ended = _ended.toList();
    _ended.clear();
    for (final id in ended) {
      _finish(id, complete: false);
    }
  }

  void _finish(int id, {required bool complete}) {
    final packets = _open.remove(id);
    if (packets == null || packets.isEmpty) return;
    onSegment(
      MockingbirdSegment(
        id: id,
        packets: List.unmodifiable(packets),
        sampleRateHz: sampleRateHz,
        complete: complete,
      ),
    );
  }
}
