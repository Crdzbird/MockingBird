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

  test('resuming after a pause while ducked restores the volume', () async {
    await player.start();
    await send(MockingbirdPlaybackFocus.duck);
    await player.pause();
    await send(MockingbirdPlaybackFocus.gained);
    await player.resume();
    expect(platform.calls.skip(2), [
      'volume 0.2',
      'pause',
      'resume',
      'volume 1.0',
    ]);
  });

  test('a delayed grant starts paused and plays once focus arrives', () async {
    platform.onRequest = MockingbirdPlaybackFocus.lostTransient;
    await player.start();
    await pumpEventQueue();
    expect(platform.calls, ['focus', 'start', 'pause']);
    await send(MockingbirdPlaybackFocus.gained);
    expect(platform.calls.skip(3), ['resume', 'volume 1.0']);
  });
}
