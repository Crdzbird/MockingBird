@TestOn('vm')
library;

import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/native_test_platform.dart';

Future<List<MockingbirdPacket>> _encode() async {
  final engine = MockingbirdEngine.create(
    const MockingbirdConfig(
      backend: .hostPcm,
      vad: MockingbirdVadConfig(enabled: false),
      anomaly: MockingbirdAnomalyConfig(enabled: false),
      encoder: MockingbirdEncoderConfig.opus(),
    ),
  );
  final packets = <MockingbirdPacket>[];
  final sub = engine.packets.listen(packets.add);
  engine.start();
  final tone = Int16List.fromList(
    List.generate(16000, (i) => (8000 * math.sin(i * 0.1728)).round()),
  );
  for (var at = 0; at < tone.length; at += 320) {
    engine.pushPcm(tone.sublist(at, at + 320), captureTsUs: at * 62);
  }
  engine.stop();
  await pumpEventQueue();
  await sub.cancel();
  await engine.dispose();
  return packets;
}

void main() {
  setUpAll(() => MockingbirdPlatform.instance = NativeTestPlatform());

  test(
    'engine packets decode and play back through the native library',
    () async {
      final packets = await _encode();
      expect(packets, hasLength(50));
      final decoder = MockingbirdDecoder.create();
      final frames = packets.expand((p) => decoder.decode(p.data)).length;
      expect(frames, 16000);
      expect(decoder.conceal(), hasLength(320));
      expect(
        () => decoder.decode(Uint8List.fromList([0xff, 0xff, 0xff])),
        throwsA(isA<MockingbirdException>()),
      );
      decoder.dispose();
      final player = MockingbirdPlayer.create(
        const MockingbirdPlayerConfig(
          backend: .hostPcm,
          maxDelay: Duration(seconds: 2),
        ),
      );
      await player.start();
      for (var i = 0; i < packets.length; i++) {
        player.pushOpus(packets[i].data, ptsUs: i * 20000, last: i == 49);
      }
      final out = player.read(16000);
      final stats = player.readStats();
      expect(out.any((s) => s != 0), isTrue);
      expect(stats.played, 50);
      expect(stats.concealed, 0);
      expect(stats.underruns, 0);
      await player.dispose();
    },
    skip: NativeTestPlatform.available ? false : 'build the library first',
  );

  test('a config the engine rejects throws invalidArg', () {
    expect(
      () => MockingbirdPlayer.create(
        const MockingbirdPlayerConfig(sampleRateHz: 44100),
      ),
      throwsA(
        isA<MockingbirdException>().having(
          (e) => e.status,
          'status',
          MockingbirdStatus.invalidArg,
        ),
      ),
    );
  }, skip: NativeTestPlatform.available ? false : 'build the library first');
}
