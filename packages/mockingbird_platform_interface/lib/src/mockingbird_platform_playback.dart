import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_native_player_config.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_playback_focus.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_player_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

/// The player operations; each default says the platform cannot play.
abstract class MockingbirdPlatformPlayback extends PlatformInterface {
  /// Passes the verification [token] up.
  MockingbirdPlatformPlayback({required super.token});

  /// Creates a player for [config].
  MockingbirdPlayerCreateResult createPlayer(
    MockingbirdNativePlayerConfig config,
  ) => (status: MockingbirdStatusCode.unsupported, handle: null);

  /// Stops the player if needed and destroys it.
  Future<void> destroyPlayer(MockingbirdPlayerHandle handle) async {}

  /// Opens the output and starts playout.
  Future<int> playerStart(MockingbirdPlayerHandle handle) async =>
      MockingbirdStatusCode.unsupported;

  /// Stops the output, keeping what is buffered.
  Future<int> playerPause(MockingbirdPlayerHandle handle) async =>
      MockingbirdStatusCode.unsupported;

  /// Starts the output again after a pause or a lost route.
  Future<int> playerResume(MockingbirdPlayerHandle handle) async =>
      MockingbirdStatusCode.unsupported;

  /// Closes the output and drops what is buffered.
  Future<int> playerStop(MockingbirdPlayerHandle handle) async =>
      MockingbirdStatusCode.unsupported;

  /// Queues one packet of `mockingbird_codec_t` [codec] at [ptsUs].
  int playerPush(
    MockingbirdPlayerHandle handle, {
    required int codec,
    required Uint8List data,
    required int ptsUs,
    required int flags,
  }) => MockingbirdStatusCode.unsupported;

  /// Drops everything pushed so far and starts a new timeline.
  int playerFlush(MockingbirdPlayerHandle handle) =>
      MockingbirdStatusCode.unsupported;

  /// Renders [frames] frames from a host-PCM player.
  MockingbirdPcmResult playerRead(MockingbirdPlayerHandle handle, int frames) =>
      (status: MockingbirdStatusCode.unsupported, samples: Int16List(0));

  /// Sets the linear gain, 0 to 1.
  int playerSetVolume(MockingbirdPlayerHandle handle, double volume) =>
      MockingbirdStatusCode.unsupported;

  /// Mutes or unmutes without changing the volume.
  int playerSetMuted(MockingbirdPlayerHandle handle, {required bool muted}) =>
      MockingbirdStatusCode.unsupported;

  /// Reads a stats snapshot.
  MockingbirdPlayerStatsResult playerStats(MockingbirdPlayerHandle handle) =>
      (status: MockingbirdStatusCode.unsupported, stats: null);

  /// Asks the operating system for the right to play, where it has one.
  Future<void> requestPlaybackFocus() async {}

  /// Gives that right back.
  Future<void> abandonPlaybackFocus() async {}

  /// Focus changes while a player holds focus; empty where there is none.
  Stream<MockingbirdPlaybackFocus> get playbackFocusChanges =>
      const Stream.empty();
}
