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
    _focusPaused = false;
    _focus.follow(_platform.playbackFocusChanges);
    await _platform.requestPlaybackFocus();
    final s = await _platform.playerStart(_handle);
    if (s != MockingbirdStatusCode.ok) {
      await _focus.cancel();
      await _platform.abandonPlaybackFocus();
      MockingbirdException.check(s, 'mockingbird_player_start');
    }
    _timer = Timer.periodic(statsInterval, (_) => _tick());
    if (_focusPaused) await _platform.playerPause(_handle);
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
    _focusPaused = false;
    MockingbirdException.check(
      await _platform.playerResume(_handle),
      'mockingbird_player_resume',
    );
    _platform.playerSetVolume(_handle, _volume);
  }

  /// Closes the output, drops what is buffered and gives focus back.
  Future<void> stop() async {
    _ensureLive();
    if (!isRunning) return;
    final s = await _halt();
    _tick();
    MockingbirdException.check(s, 'mockingbird_player_stop');
  }

  Future<int> _halt() async {
    final pending = _halting;
    if (pending != null) return await pending;
    if (!isRunning) return MockingbirdStatusCode.ok;
    final halting = _haltOnce();
    _halting = halting;
    try {
      return await halting;
    } finally {
      _halting = null;
    }
  }

  Future<int> _haltOnce() async {
    _timer?.cancel();
    _timer = null;
    _focusPaused = false;
    final s = await _platform.playerStop(_handle);
    await _focus.cancel();
    await _platform.abandonPlaybackFocus();
    return s;
  }

  Future<void> _pauseForFocus() async {
    _focusPaused = true;
    if (isRunning) await _platform.playerPause(_handle);
  }

  Future<void> _resumeAfterFocus() async {
    _focusPaused = false;
    if (isRunning) await _platform.playerResume(_handle);
    _platform.playerSetVolume(_handle, _volume);
  }

  void _duck() => _platform.playerSetVolume(_handle, _volume * 0.2);
}
