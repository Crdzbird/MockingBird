import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'host_audio.dart';

Future<List<MockingbirdPacket>> _opus(int frames) async {
  final engine = MockingbirdEngine.create(
    const MockingbirdConfig(
      backend: .hostPcm,
      vad: MockingbirdVadConfig(enabled: false),
      anomaly: MockingbirdAnomalyConfig(enabled: false),
      encoder: MockingbirdEncoderConfig.opus(),
    ),
  )..start();
  final packets = <MockingbirdPacket>[];
  final sub = engine.packets.listen(packets.add);
  await push(engine, frames, amplitude: 6000);
  await Future<void>.delayed(const Duration(milliseconds: 400));
  engine.stop();
  await sub.cancel();
  await engine.dispose();
  return packets;
}

/// Opus decoding and playback on the device's real output, muted.
void playerTests() {
  test('engine packets decode back to one frame each', () async {
    final packets = await _opus(25);
    expect(packets, hasLength(25));
    final decoder = MockingbirdDecoder.create();
    addTearDown(decoder.dispose);
    for (final p in packets) {
      expect(decoder.decode(p.data), hasLength(320));
    }
    expect(decoder.conceal(), hasLength(320));
  });

  test('the player plays engine packets on the output', () async {
    final packets = await _opus(50);
    final player = MockingbirdPlayer.create(
      const MockingbirdPlayerConfig(maxDelay: Duration(seconds: 2)),
    )..muted = true;
    addTearDown(player.dispose);
    await player.start();
    packets.forEach(player.push);
    await Future<void>.delayed(const Duration(milliseconds: 1600));
    final s = player.readStats();
    expect(s.played, 50);
    expect(s.framesRendered, greaterThan(16000));
    await player.pause();
    expect(player.readStats().state, MockingbirdPlayerState.paused);
    await player.resume();
    await player.stop();
    expect(player.readStats().state, MockingbirdPlayerState.stopped);
  });
}
