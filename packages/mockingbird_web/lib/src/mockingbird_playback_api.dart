import 'dart:js_interop';

/// The module's player entry points. The player is created and fed through
/// the shim, which forces host PCM and passes pts as a double.
extension type MockingbirdPlaybackApi(JSObject _) implements JSObject {
  /// Creates a host-PCM player; the status lands at [outStatus].
  @JS('_mockingbird_web_player_create')
  external int playerCreate(
    int rate,
    int channels,
    int targetMs,
    int maxMs,
    int concealMs,
    int queue,
    int outStatus,
  );

  /// Destroys a player.
  @JS('_mockingbird_player_destroy')
  external void playerDestroy(int player);

  /// Starts playout.
  @JS('_mockingbird_player_start')
  external int playerStart(int player);

  /// Pauses playout.
  @JS('_mockingbird_player_pause')
  external int playerPause(int player);

  /// Resumes playout.
  @JS('_mockingbird_player_resume')
  external int playerResume(int player);

  /// Stops and drops what is buffered.
  @JS('_mockingbird_player_stop')
  external int playerStop(int player);

  /// Starts a new timeline.
  @JS('_mockingbird_player_flush')
  external int playerFlush(int player);

  /// Queues [size] bytes at [data].
  @JS('_mockingbird_web_player_push')
  external int playerPush(
    int player,
    int codec,
    int data,
    int size,
    double ptsUs,
    int flags,
  );

  /// Renders [frames] frames at [out].
  @JS('_mockingbird_player_read')
  external int playerRead(int player, int out, int frames);

  /// Sets the gain.
  @JS('_mockingbird_player_set_volume')
  external int playerSetVolume(int player, double volume);

  /// Mutes (1) or unmutes (0).
  @JS('_mockingbird_player_set_muted')
  external int playerSetMuted(int player, int muted);

  /// One stats field, in `mockingbird_player_stats_t` order; -1 on failure.
  @JS('_mockingbird_web_player_stat')
  external double playerStat(int player, int field);
}
