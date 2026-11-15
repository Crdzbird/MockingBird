part of 'mockingbird_player.dart';

/// Starting, pausing and stopping a [MockingbirdPlayer]. The output opens
/// and closes on the player's own thread, so these calls never block this
/// isolate, and they run in the order they were made: a [stop] made while
/// [start] is still opening the output runs once it is open.
extension MockingbirdPlayerLifecycle on MockingbirdPlayer {
  /// Takes audio focus where the platform has it, opens the output and
  /// starts playout; completes once the output plays. Packets pushed before
  /// then wait in the player's queue and play once it does. [stats] ticks
  /// every [statsInterval]. On the web, call it from a user gesture so the
  /// browser lets the page play.
  Future<void> start({
    Duration statsInterval = const Duration(milliseconds: 250),
  }) async {
    _ensureLive();
    await _queue.add(() => _start(statsInterval));
  }

  /// Stops the output and keeps what is buffered.
  Future<void> pause() async {
    _ensureLive();
    _focus.userPaused();
    await _queue.add(
      () async => MockingbirdException.check(
        await _platform.playerPause(_handle),
        'mockingbird_player_pause',
      ),
    );
  }

  /// Starts the output again after [pause], audio focus or a lost route.
  Future<void> resume() async {
    _ensureLive();
    _focus.userResumed();
    _focusPaused = false;
    await _queue.add(() async {
      MockingbirdException.check(
        await _platform.playerResume(_handle),
        'mockingbird_player_resume',
      );
      _platform.playerSetVolume(_handle, _volume);
    });
  }

  /// Closes the output, drops what is buffered and gives focus back.
  Future<void> stop() async {
    _ensureLive();
    await _queue.add(() async {
      if (!isRunning) return;
      final s = await _halt();
      _tick();
      MockingbirdException.check(s, 'mockingbird_player_stop');
    });
  }

  Future<void> _start(Duration statsInterval) async {
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

  Future<int> _halt() async {
    if (!isRunning) return MockingbirdStatusCode.ok;
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
