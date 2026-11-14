import 'dart:async';

import 'package:mockingbird/mockingbird.dart';

/// Keeps the last ten seconds of captured packets and plays them back
/// through a [MockingbirdPlayer] at their capture times, so the gaps
/// between segments close up and each segment plays at its own pace. The
/// player gets a slot for every packet, since they are all pushed at once.
final class CaptureReplay {
  factory CaptureReplay() = CaptureReplay._;

  CaptureReplay._();

  static const _keepUs = 10000000;
  final _packets = <MockingbirdPacket>[];

  /// How much audio is kept, by the packets' capture times.
  Duration get kept => _packets.isEmpty
      ? Duration.zero
      : Duration(
          microseconds: _packets.last.captureTsUs - _packets.first.captureTsUs,
        );

  /// Keeps [packet], dropping what is older than ten seconds.
  void keep(MockingbirdPacket packet) {
    _packets.add(packet);
    final oldest = packet.captureTsUs - _keepUs;
    _packets.removeWhere((p) => p.captureTsUs < oldest);
  }

  /// Plays everything kept and completes once the player has gone quiet.
  Future<void> play(void Function(MockingbirdPlayerStats) onStats) async {
    if (_packets.isEmpty) return;
    final player = MockingbirdPlayer.create(
      MockingbirdPlayerConfig(
        maxDelay: const Duration(seconds: 10),
        queuePackets: (_packets.length + 4).clamp(128, 4096),
      ),
    );
    final done = Completer<void>();
    final sub = player.stats.listen((s) {
      onStats(s);
      final settled = s.played + s.concealed + s.dropped >= _packets.length;
      if (settled && s.state == .idle && !done.isCompleted) done.complete();
    });
    try {
      await player.start(statsInterval: const Duration(milliseconds: 100));
      _packets.take(_packets.length - 1).forEach(player.push);
      final last = _packets.last;
      player.pushOpus(last.data, ptsUs: last.captureTsUs, last: true);
      await done.future.timeout(kept + const Duration(seconds: 2));
    } on TimeoutException {
      return;
    } finally {
      await sub.cancel();
      await player.dispose();
    }
  }
}
