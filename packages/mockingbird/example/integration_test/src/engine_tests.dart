import 'dart:math';

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'host_audio.dart';

/// Engine lifecycle, packets, stats, the PCM tap and live retuning.
void engineTests() {
  test('the native ABI resolves through dart:ffi', () {
    expect(RegExp(r'^\d+\.\d+\.\d+').hasMatch(Mockingbird.version), isTrue);
  });

  test('passthrough emits one packet per pushed frame', () async {
    final engine = MockingbirdEngine.create(hostConfig())..start();
    addTearDown(engine.dispose);
    final packets = <MockingbirdPacket>[];
    addTearDown(engine.packets.listen(packets.add).cancel);
    await push(engine, 10);
    await Future<void>.delayed(const Duration(milliseconds: 400));
    expect(packets.length, 10);
    expect(packets.fold<int>(0, (n, p) => n + p.data.length), 10 * 320 * 2);
  });

  test('native platforms push packets without waiting for the timer', () async {
    final engine = MockingbirdEngine.create(hostConfig())
      ..start(pollInterval: const Duration(seconds: 5));
    addTearDown(engine.dispose);
    final first = engine.packets.first;
    engine.pushPcm(frame());
    final packet = await first.timeout(const Duration(seconds: 1));
    expect(packet.data.length, 320 * 2);
  }, skip: kIsWeb);

  test('segments arrive as playable Ogg Opus files', () async {
    final engine = MockingbirdEngine.create(
      const MockingbirdConfig(
        backend: .hostPcm,
        vad: MockingbirdVadConfig(enabled: false),
        anomaly: MockingbirdAnomalyConfig(enabled: false),
        segments: MockingbirdSegmentConfig(maxMs: 200),
        encoder: MockingbirdEncoderConfig.oggOpus(),
      ),
    )..start();
    final segments = <MockingbirdSegment>[];
    final sub = engine.segments.listen(segments.add);
    await push(engine, 25, amplitude: 6000);
    await Future<void>.delayed(const Duration(milliseconds: 300));
    engine.stop();
    await engine.dispose();
    await sub.cancel();
    expect(segments.map((s) => s.complete), [true, true, true]);
    for (final s in segments) {
      expect(String.fromCharCodes(s.toFile().sublist(0, 4)), 'OggS');
    }
  });

  test('stats report the input level; the PCM tap is ungated', () async {
    final engine = MockingbirdEngine.create(hostConfig(tap: true))..start();
    addTearDown(engine.dispose);
    const amp = 8192;
    await push(engine, 10, amplitude: amp);
    await Future<void>.delayed(const Duration(milliseconds: 400));
    final stats = await engine.stats.first;
    expect(stats.inputRmsDbfs, closeTo(20 * log(amp / 32768) / ln10, 0.5));
    var tapped = 0;
    for (var c = engine.readPcm(); c.isNotEmpty; c = engine.readPcm()) {
      tapped += c.length;
    }
    expect(tapped, 10 * 320);
  });

  test('a live retune applies; a structural change is rejected', () async {
    final config = hostConfig(anomaly: const MockingbirdAnomalyConfig());
    final engine = MockingbirdEngine.create(config)..start();
    addTearDown(engine.dispose);
    await push(engine, 5, amplitude: 4000);
    engine.updateConfig(config);
    expect(engine.isRunning, isTrue);
    expect(
      () => engine.updateConfig(const MockingbirdConfig(backend: .hostPcm)),
      throwsA(isA<MockingbirdException>()),
    );
  });
}
