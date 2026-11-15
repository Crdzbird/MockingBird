@TestOn('vm')
library;

import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/native_test_platform.dart';

Object get _skip =>
    NativeTestPlatform.available ? false : 'build the library first';

MockingbirdEngine _engine() => MockingbirdEngine.create(
  const MockingbirdConfig(
    backend: .hostPcm,
    vad: MockingbirdVadConfig(enabled: false),
    anomaly: MockingbirdAnomalyConfig(enabled: false),
    encoder: MockingbirdEncoderConfig.opus(),
  ),
);

void main() {
  setUpAll(() => MockingbirdPlatform.instance = NativeTestPlatform());

  test('startAsync and stopAsync run capture on the native thread', () async {
    final engine = _engine();
    final packets = <MockingbirdPacket>[];
    final sub = engine.packets.listen(packets.add);
    await engine.startAsync();
    expect(engine.isRunning, isTrue);
    for (var at = 0; at < 16000; at += 320) {
      engine.pushPcm(Int16List(320)..fillRange(0, 320, 900), captureTsUs: at);
    }
    await engine.stopAsync();
    expect(engine.isRunning, isFalse);
    await pumpEventQueue();
    expect(packets, hasLength(50));
    await sub.cancel();
    await engine.dispose();
  }, skip: _skip);

  test('engine calls made without awaiting run in order', () async {
    final engine = _engine();
    final calls = [
      engine.startAsync(),
      engine.stopAsync(),
      engine.startAsync(),
    ];
    await Future.wait(calls);
    expect(engine.isRunning, isTrue);
    await engine.dispose();
    expect(engine.isRunning, isFalse);
  }, skip: _skip);

  test('a player plays packets pushed before its start completes', () async {
    final packets = <MockingbirdPacket>[];
    final engine = _engine();
    final sub = engine.packets.listen(packets.add);
    await engine.startAsync();
    for (var at = 0; at < 16000; at += 320) {
      engine.pushPcm(Int16List(320)..fillRange(0, 320, 900), captureTsUs: at);
    }
    await engine.stopAsync();
    await pumpEventQueue();
    await sub.cancel();
    await engine.dispose();
    expect(packets, hasLength(50));
    final player = MockingbirdPlayer.create(
      const MockingbirdPlayerConfig(
        backend: .hostPcm,
        maxDelay: Duration(seconds: 2),
      ),
    );
    final starting = player.start();
    for (var i = 0; i < packets.length; i++) {
      player.pushOpus(packets[i].data, ptsUs: i * 20000, last: i == 49);
    }
    await starting;
    player.read(16000);
    expect(player.readStats().played, 50);
    final calls = [player.stop(), player.start(), player.stop()];
    await Future.wait(calls);
    expect(player.isRunning, isFalse);
    expect(player.readStats().state, MockingbirdPlayerState.stopped);
    await player.dispose();
  }, skip: _skip);
}
