import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/fake_player_platform.dart';

void main() {
  late FakePlayerPlatform platform;
  late MockingbirdPlayer player;
  late Completer<void> opening;

  setUp(() {
    opening = Completer<void>();
    platform = FakePlayerPlatform()..opening = opening;
    MockingbirdPlatform.instance = platform;
    player = MockingbirdPlayer.create();
  });

  tearDown(() async {
    if (!opening.isCompleted) opening.complete();
    await player.dispose();
  });

  test('start yields to the event loop while the output opens', () async {
    var started = false;
    final starting = player.start().then((_) => started = true);
    await pumpEventQueue();
    expect(platform.calls, ['focus', 'start']);
    expect(started, isFalse);
    expect(player.isRunning, isFalse);
    opening.complete();
    await starting;
    expect(player.isRunning, isTrue);
  });

  test('packets pushed while the output opens reach the player', () async {
    final starting = player.start();
    await pumpEventQueue();
    player.pushOpus(Uint8List(3), ptsUs: 0);
    expect(platform.calls, ['focus', 'start', 'push']);
    opening.complete();
    await starting;
  });

  test('a stop made while the output opens runs after the start', () async {
    final starting = player.start();
    final stopping = player.stop();
    await pumpEventQueue();
    expect(platform.calls, ['focus', 'start']);
    opening.complete();
    await Future.wait([starting, stopping]);
    expect(platform.calls, ['focus', 'start', 'stop', 'abandon']);
    expect(player.isRunning, isFalse);
  });

  test('pause and resume made during the start run after it', () async {
    final calls = [player.start(), player.pause(), player.resume()];
    opening.complete();
    await Future.wait(calls);
    expect(platform.calls, ['focus', 'start', 'pause', 'resume', 'volume 1.0']);
  });

  test('two overlapping starts open the output once', () async {
    final calls = [player.start(), player.start()];
    opening.complete();
    await Future.wait(calls);
    expect(platform.calls.where((c) => c == 'start'), hasLength(1));
  });

  test('dispose during the start stops after it, then destroys', () async {
    final starting = player.start();
    final disposing = player.dispose();
    opening.complete();
    await Future.wait([starting, disposing]);
    expect(platform.calls, ['focus', 'start', 'stop', 'abandon', 'destroy']);
  });

  test('a failed start throws and later calls still run', () async {
    opening.complete();
    platform.startStatus = MockingbirdStatusCode.unsupported;
    await expectLater(player.start(), throwsA(isA<MockingbirdException>()));
    platform.startStatus = MockingbirdStatusCode.ok;
    await player.start();
    expect(player.isRunning, isTrue);
    expect(platform.calls, ['focus', 'start', 'abandon', 'focus', 'start']);
  });
}
