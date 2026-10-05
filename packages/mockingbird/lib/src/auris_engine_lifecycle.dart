part of 'auris_engine.dart';

/// Starting and stopping an [AurisEngine].
extension AurisEngineLifecycle on AurisEngine {
  /// Starts capture and polls the engine every [pollInterval].
  void start({Duration pollInterval = const Duration(milliseconds: 50)}) {
    _ensureLive();
    if (isRunning) return;
    AurisException.check(_platform.start(_handle), 'auris_start');
    _timer = Timer.periodic(pollInterval, (_) => _poll());
  }

  /// Stops capture and drains the final events.
  void stop() {
    _ensureLive();
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    AurisException.check(_platform.stop(_handle), 'auris_stop');
    _poll();
  }
}
