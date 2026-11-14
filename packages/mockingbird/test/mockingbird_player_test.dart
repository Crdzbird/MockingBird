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

  test(
    'dispose is idempotent and a disposed player throws StateError',
    () async {
      await player.dispose();
      await player.dispose();
      expect(platform.calls.where((c) => c == 'destroy'), hasLength(1));
      expect(() => player.pushOpus(Uint8List(1), ptsUs: 0), throwsStateError);
    },
  );

  test('overlapping dispose calls destroy the native player once', () async {
    await player.start();
    await Future.wait([player.dispose(), player.dispose()]);
    expect(platform.calls.where((c) => c == 'destroy'), hasLength(1));
    expect(platform.calls.last, 'destroy');
  });
}
