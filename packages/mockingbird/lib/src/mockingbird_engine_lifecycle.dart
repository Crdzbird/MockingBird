part of 'mockingbird_engine.dart';

/// Starting and stopping a [MockingbirdEngine].
extension MockingbirdEngineLifecycle on MockingbirdEngine {
  /// Starts capture. On native platforms the engine wakes Dart as soon as an
  /// event or packet is queued, and [pollInterval] (250 ms by default) only
  /// paces [stats]. On the web every [pollInterval] (50 ms by default)
  /// drains the queues and reads the stats.
  void start({Duration? pollInterval}) {
    _ensureLive();
    if (isRunning) return;
    MockingbirdException.check(_platform.start(_handle), 'mockingbird_start');
    final pushed = _platform.setWakeListener(_handle, _wake);
    final every = pollInterval ?? Duration(milliseconds: pushed ? 250 : 50);
    _timer = Timer.periodic(every, (_) => _poll());
    if (pushed) _wake();
  }

  /// Stops capture and drains the final events.
  void stop() {
    _ensureLive();
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    MockingbirdException.check(_platform.stop(_handle), 'mockingbird_stop');
    _poll();
    _platform.setWakeListener(_handle, null);
  }
}
