/// What a `MockingbirdPlayer` is doing; `mockingbird_player_state_t`.
enum MockingbirdPlayerState {
  /// Not started, or stopped.
  stopped(0),

  /// Started with nothing to play.
  idle(1),

  /// Waiting for the target delay's worth of audio.
  buffering(2),

  /// Playing.
  playing(3),

  /// Paused by the app, by audio focus, or by the route going away.
  paused(4),

  /// The operating system took the output away, a phone call for example.
  interrupted(5);

  const MockingbirdPlayerState(this.nativeValue);

  /// The native value.
  final int nativeValue;

  /// Decodes a native state; unknown values map to [stopped].
  static MockingbirdPlayerState fromNative(int value) =>
      values.firstWhere((s) => s.nativeValue == value, orElse: () => stopped);
}
