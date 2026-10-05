@TestOn('browser')
library;

import 'dart:math';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/mockingbird_web.dart';
import 'package:flutter_test/flutter_test.dart';

import 'test_config.dart';

Int16List bursts(int seconds) {
  final rng = Random(7);
  final out = Int16List(16000 * seconds);
  for (var i = 0; i < out.length; i++) {
    final loud = (i ~/ 8000).isOdd;
    out[i] = loud ? (rng.nextDouble() * 20000 - 10000).round() : 0;
  }
  return out;
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late AurisWeb platform;

  setUpAll(() async {
    AurisWeb.assetBaseUrl = Uri.base.resolve('wasm/').toString();
    platform = AurisWeb();
    AurisPlatform.instance = platform;
    await platform.initialize();
  });

  test('the module loads and reports the engine version', () {
    expect(platform.version(), matches(RegExp(r'^\d+\.\d+\.\d+$')));
    expect(platform.statusName(AurisStatusCode.queueEmpty), isNotEmpty);
  });

  test('a host-PCM engine processes pushed audio', () {
    final created = platform.create(testConfig());
    expect(created.status, AurisStatusCode.ok);
    final h = created.handle!;
    expect(platform.start(h), AurisStatusCode.ok);
    final pcm = bursts(4);
    var pushed = 0;
    var packets = 0;
    final types = <int>{};
    for (var i = 0, ts = 0; i + 320 <= pcm.length; i += 320, ts += 20000) {
      final s = platform.pushPcm(h, Int16List.sublistView(pcm, i, i + 320), ts);
      expect(s, AurisStatusCode.ok);
      pushed += 320;
      for (var e = platform.pollEvent(h).event;
          e != null;
          e = platform.pollEvent(h).event) {
        types.add(e.type);
      }
      for (var p = platform.drainPacket(h).packet;
          p != null;
          p = platform.drainPacket(h).packet) {
        packets++;
        expect(p.data, isNotEmpty);
      }
    }
    final stats = platform.stats(h).stats!;
    expect(stats.framesCaptured, pushed);
    expect(stats.framesDroppedCapture, 0);
    expect(packets, greaterThan(0));
    expect(types, contains(4));
    expect(platform.stop(h), AurisStatusCode.ok);
    platform.destroy(h);
  });

  test('a capture engine receives frames from the microphone', () async {
    final created = platform.create(testConfig(backend: 0));
    expect(created.status, AurisStatusCode.ok);
    final h = created.handle!;
    expect(platform.start(h), AurisStatusCode.ok);
    final deadline = DateTime.now().add(const Duration(seconds: 8));
    var captured = 0;
    while (captured == 0 && DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 100));
      final e = platform.pollEvent(h).event;
      expect(e?.type, isNot(7), reason: 'capture reported a backend error');
      captured = platform.stats(h).stats!.framesCaptured;
    }
    expect(captured, greaterThan(0));
    expect(platform.stop(h), AurisStatusCode.ok);
    platform.destroy(h);
  });
}
