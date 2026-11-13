import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_web_decoding.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_player_controls.dart';
import 'package:mockingbird_web/src/web_player_feed.dart';
import 'package:mockingbird_web/src/web_player_handle.dart';
import 'package:mockingbird_web/src/web_players.dart';

/// The player over WebAssembly, played through Web Audio.
abstract class MockingbirdWebPlayback extends MockingbirdWebDecoding {
  WebPlayers get _players => WebPlayers(ModuleRegistry.module);

  WebPlayerControls get _controls => WebPlayerControls(ModuleRegistry.module);

  @override
  MockingbirdPlayerCreateResult createPlayer(
    MockingbirdNativePlayerConfig config,
  ) => _players.create(config);

  @override
  Future<void> destroyPlayer(MockingbirdPlayerHandle handle) =>
      _players.destroy(_p(handle));

  @override
  Future<int> playerStart(MockingbirdPlayerHandle handle) =>
      _players.start(_p(handle));

  @override
  Future<int> playerPause(MockingbirdPlayerHandle handle) =>
      _players.pause(_p(handle));

  @override
  Future<int> playerResume(MockingbirdPlayerHandle handle) =>
      _players.resume(_p(handle));

  @override
  Future<int> playerStop(MockingbirdPlayerHandle handle) =>
      _players.stop(_p(handle));

  @override
  int playerPush(
    MockingbirdPlayerHandle handle, {
    required int codec,
    required Uint8List data,
    required int ptsUs,
    required int flags,
  }) => WebPlayerFeed(
    ModuleRegistry.module,
  ).push(_p(handle), codec, data, ptsUs, flags);

  @override
  int playerFlush(MockingbirdPlayerHandle handle) =>
      _controls.flush(_p(handle));

  @override
  MockingbirdPcmResult playerRead(MockingbirdPlayerHandle handle, int frames) =>
      WebPlayerFeed(ModuleRegistry.module).read(_p(handle), frames);

  @override
  int playerSetVolume(MockingbirdPlayerHandle handle, double volume) =>
      _controls.volume(_p(handle), volume);

  @override
  int playerSetMuted(MockingbirdPlayerHandle handle, {required bool muted}) =>
      _controls.muted(_p(handle), muted: muted);

  @override
  MockingbirdPlayerStatsResult playerStats(MockingbirdPlayerHandle handle) =>
      _controls.stats(_p(handle));

  static WebPlayerHandle _p(MockingbirdPlayerHandle h) => h as WebPlayerHandle;
}
