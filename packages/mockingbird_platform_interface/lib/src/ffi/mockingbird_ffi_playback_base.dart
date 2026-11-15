import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_player_feed.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_player_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_players.dart';
import 'package:mockingbird_platform_interface/src/ffi/mockingbird_ffi_decoding_base.dart';

/// The player operations over `dart:ffi`.
abstract class MockingbirdFfiPlaybackBase extends MockingbirdFfiDecodingBase {
  late final FfiPlayers _players = FfiPlayers(bindings);
  late final FfiPlayerFeed _feed = FfiPlayerFeed(bindings);

  @override
  MockingbirdPlayerCreateResult createPlayer(
    MockingbirdNativePlayerConfig config,
  ) => _players.create(config);

  @override
  Future<void> destroyPlayer(MockingbirdPlayerHandle handle) async {
    await _players.run(_p(handle), .MOCKINGBIRD_PLAYER_OP_CLOSE);
    _players.destroy(_p(handle));
  }

  @override
  Future<int> playerStart(MockingbirdPlayerHandle handle) =>
      _players.run(_p(handle), .MOCKINGBIRD_PLAYER_OP_START);

  @override
  Future<int> playerPause(MockingbirdPlayerHandle handle) =>
      _players.run(_p(handle), .MOCKINGBIRD_PLAYER_OP_PAUSE);

  @override
  Future<int> playerResume(MockingbirdPlayerHandle handle) =>
      _players.run(_p(handle), .MOCKINGBIRD_PLAYER_OP_RESUME);

  @override
  Future<int> playerStop(MockingbirdPlayerHandle handle) =>
      _players.run(_p(handle), .MOCKINGBIRD_PLAYER_OP_STOP);

  @override
  int playerFlush(MockingbirdPlayerHandle handle) => _players.flush(_p(handle));

  @override
  int playerPush(
    MockingbirdPlayerHandle handle, {
    required int codec,
    required Uint8List data,
    required int ptsUs,
    required int flags,
  }) => _feed.push(
    _p(handle),
    codec: codec,
    data: data,
    ptsUs: ptsUs,
    flags: flags,
  );

  @override
  MockingbirdPcmResult playerRead(MockingbirdPlayerHandle handle, int frames) =>
      _feed.read(_p(handle), frames);

  @override
  int playerSetVolume(MockingbirdPlayerHandle handle, double volume) =>
      _players.volume(_p(handle), volume);

  @override
  int playerSetMuted(MockingbirdPlayerHandle handle, {required bool muted}) =>
      _players.muted(_p(handle), muted: muted);

  @override
  MockingbirdPlayerStatsResult playerStats(MockingbirdPlayerHandle handle) =>
      _players.stats(_p(handle));

  static FfiPlayerHandle _p(MockingbirdPlayerHandle h) => h as FfiPlayerHandle;
}
