part of 'mockingbird_engine.dart';

/// Starting and stopping a [MockingbirdEngine].
extension MockingbirdEngineLifecycle on MockingbirdEngine {
  /// Starts capture and polls the engine every [pollInterval].
  void start({Duration pollInterval = const Duration(milliseconds: 50)}) {
    _ensureLive();
    if (isRunning) return;
    MockingbirdException.check(_platform.start(_handle), 'mockingbird_start');
    _timer = Timer.periodic(pollInterval, (_) => _poll());
  }

  /// Stops capture and drains the final events.
  void stop() {
    _ensureLive();
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    MockingbirdException.check(_platform.stop(_handle), 'mockingbird_stop');
    _poll();
  }
}
