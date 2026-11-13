part of 'mockingbird_player.dart';

/// Volume and mute of a [MockingbirdPlayer]; both ramp over 10 ms.
extension MockingbirdPlayerControls on MockingbirdPlayer {
  /// The linear gain, 0 to 1.
  double get volume => _volume;

  /// Sets the linear gain, 0 to 1.
  set volume(double value) {
    _ensureLive();
    MockingbirdException.check(
      _platform.playerSetVolume(_handle, value),
      'mockingbird_player_set_volume',
    );
    _volume = value;
  }

  /// Whether the output is muted; muting keeps [volume].
  bool get muted => _muted;

  /// Mutes or unmutes.
  set muted(bool value) {
    _ensureLive();
    MockingbirdException.check(
      _platform.playerSetMuted(_handle, muted: value),
      'mockingbird_player_set_muted',
    );
    _muted = value;
  }
}
