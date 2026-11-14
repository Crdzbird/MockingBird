import 'dart:async';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'fake_player_handle.dart';

/// Records the player calls the app makes and lets a test drive focus.
final class FakePlayerPlatform extends MockingbirdPlatform {
  /// Creates the fake.
  factory FakePlayerPlatform() = FakePlayerPlatform._;

  FakePlayerPlatform._();

  /// Every player call, by name, in order.
  final calls = <String>[];

  /// The status the next push returns.
  int pushStatus = MockingbirdStatusCode.ok;

  /// The pts and flags of the last push.
  ({int ptsUs, int flags, int codec})? lastPush;

  /// Focus changes the test sends.
  final focus = StreamController<MockingbirdPlaybackFocus>.broadcast();

  /// A change sent while focus is requested, as a delayed grant does.
  MockingbirdPlaybackFocus? onRequest;

  @override
  MockingbirdPlayerCreateResult createPlayer(MockingbirdNativePlayerConfig c) =>
      (status: MockingbirdStatusCode.ok, handle: const FakePlayerHandle());

  @override
  Future<void> destroyPlayer(MockingbirdPlayerHandle handle) async =>
      calls.add('destroy');

  @override
  Future<int> playerStart(MockingbirdPlayerHandle handle) async =>
      _call('start');

  @override
  Future<int> playerPause(MockingbirdPlayerHandle handle) async =>
      _call('pause');

  @override
  Future<int> playerResume(MockingbirdPlayerHandle handle) async =>
      _call('resume');

  @override
  Future<int> playerStop(MockingbirdPlayerHandle handle) async => _call('stop');

  @override
  int playerSetVolume(MockingbirdPlayerHandle handle, double volume) =>
      _call('volume $volume');

  @override
  int playerPush(
    MockingbirdPlayerHandle handle, {
    required int codec,
    required Uint8List data,
    required int ptsUs,
    required int flags,
  }) {
    lastPush = (ptsUs: ptsUs, flags: flags, codec: codec);
    return pushStatus;
  }

  @override
  Future<void> requestPlaybackFocus() async {
    calls.add('focus');
    if (onRequest case final change?) focus.add(change);
  }

  @override
  Future<void> abandonPlaybackFocus() async => calls.add('abandon');

  @override
  Stream<MockingbirdPlaybackFocus> get playbackFocusChanges => focus.stream;

  int _call(String name) {
    calls.add(name);
    return MockingbirdStatusCode.ok;
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}
