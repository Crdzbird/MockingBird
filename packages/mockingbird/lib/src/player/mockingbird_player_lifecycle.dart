part of 'mockingbird_player.dart';

/// Starting, pausing and stopping a [MockingbirdPlayer].
extension MockingbirdPlayerLifecycle on MockingbirdPlayer {
  /// Takes audio focus where the platform has it, opens the output and
  /// starts playout; [stats] ticks every [statsInterval]. On the web, call
  /// it from a user gesture so the browser lets the page play.
  Future<void> start({
    Duration statsInterval = const Duration(milliseconds: 250),
  }) async {
    _ensureLive();
    if (isRunning) return;
    await _platform.requestPlaybackFocus();
    final s = await _platform.playerStart(_handle);
    if (s != MockingbirdStatusCode.ok) {
      await _platform.abandonPlaybackFocus();
      MockingbirdException.check(s, 'mockingbird_player_start');
    }
    _focus.follow(_platform.playbackFocusChanges);
    _timer = Timer.periodic(statsInterval, (_) => _tick());
  }

  /// Stops the output and keeps what is buffered.
  Future<void> pause() async {
    _ensureLive();
    _focus.userPaused();
    MockingbirdException.check(
      await _platform.playerPause(_handle),
      'mockingbird_player_pause',
    );
  }

  /// Starts the output again after [pause], audio focus or a lost route.
  Future<void> resume() async {
    _ensureLive();
    _focus.userResumed();
    MockingbirdException.check(
      await _platform.playerResume(_handle),
      'mockingbird_player_resume',
    );
  }

  /// Closes the output, drops what is buffered and gives focus back.
  Future<void> stop() async {
    _ensureLive();
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    MockingbirdException.check(
      await _platform.playerStop(_handle),
      'mockingbird_player_stop',
    );
    await _focus.cancel();
    await _platform.abandonPlaybackFocus();
    _tick();
  }

  Future<void> _pauseForFocus() async {
    if (isRunning) await _platform.playerPause(_handle);
  }

  Future<void> _resumeAfterFocus() async {
    if (isRunning) await _platform.playerResume(_handle);
    _platform.playerSetVolume(_handle, _volume);
  }

  void _duck() => _platform.playerSetVolume(_handle, _volume * 0.2);
}
