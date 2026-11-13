import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/fake_player_platform.dart';

void main() {
  late FakePlayerPlatform platform;
  late MockingbirdPlayer player;

  setUp(() {
    platform = FakePlayerPlatform();
    MockingbirdPlatform.instance = platform;
    player = MockingbirdPlayer.create();
  });

  tearDown(() => player.dispose());

  Future<void> send(MockingbirdPlaybackFocus f) async {
    platform.focus.add(f);
    await pumpEventQueue();
  }

  test(
    'start takes focus before the output; stop gives it back after',
    () async {
      await player.start();
      expect(player.isRunning, isTrue);
      await player.stop();
      expect(platform.calls, ['focus', 'start', 'stop', 'abandon']);
    },
  );

  test('an engine packet plays at its capture time with its flags', () {
    player.push(
      MockingbirdPacket(
        segmentId: 3,
        ptsUs: 40000,
        captureTsUs: 9000000,
        flags: 8,
        codec: MockingbirdCodec.opus,
        data: Uint8List(3),
      ),
    );
    expect(platform.lastPush, (ptsUs: 9000000, flags: 8, codec: 1));
    player.pushOpus(Uint8List(3), ptsUs: 20, last: true);
    expect(platform.lastPush, (ptsUs: 20, flags: 8, codec: 1));
    player.pushPcm(Int16List(320), ptsUs: 40);
    expect(platform.lastPush, (ptsUs: 40, flags: 0, codec: 0));
  });

  test('a rejected push throws a MockingbirdException with its status', () {
    platform.pushStatus = -9;
    expect(
      () => player.pushOpus(Uint8List(1), ptsUs: 0),
      throwsA(
        isA<MockingbirdException>()
            .having((e) => e.status, 'status', MockingbirdStatus.unsupported)
            .having((e) => e.call, 'call', 'mockingbird_player_push'),
      ),
    );
  });

  test('a transient focus loss pauses and focus coming back resumes', () async {
    await player.start();
    await send(MockingbirdPlaybackFocus.lostTransient);
    await send(MockingbirdPlaybackFocus.gained);
    expect(platform.calls.skip(2), ['pause', 'resume', 'volume 1.0']);
  });

  test('focus coming back does not resume what the app paused', () async {
    await player.start();
    await player.pause();
    await send(MockingbirdPlaybackFocus.lostTransient);
    await send(MockingbirdPlaybackFocus.gained);
    expect(platform.calls.skip(2), ['pause', 'pause']);
  });

  test(
    'a lost route pauses until the app resumes; ducking lowers volume',
    () async {
      await player.start();
      await send(MockingbirdPlaybackFocus.becomingNoisy);
      await send(MockingbirdPlaybackFocus.gained);
      await send(MockingbirdPlaybackFocus.duck);
      expect(platform.calls.skip(2), ['pause', 'volume 0.2']);
    },
  );

  test(
    'dispose is idempotent and a disposed player throws StateError',
    () async {
      await player.dispose();
      await player.dispose();
      expect(platform.calls.where((c) => c == 'destroy'), hasLength(1));
      expect(() => player.pushOpus(Uint8List(1), ptsUs: 0), throwsStateError);
    },
  );
}
