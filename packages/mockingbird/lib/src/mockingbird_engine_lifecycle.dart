part of 'mockingbird_engine.dart';

/// Starting and stopping a [MockingbirdEngine].
extension MockingbirdEngineLifecycle on MockingbirdEngine {
  /// Starts capture. On native platforms the engine wakes Dart as soon as an
  /// event or packet is queued, and [pollInterval] (250 ms by default) only
  /// paces [stats]. On the web every [pollInterval] (50 ms by default)
  /// drains the queues and reads the stats. Opening the microphone blocks
  /// this isolate meanwhile; [startAsync] does not.
  void start({Duration? pollInterval}) {
    _ensureLive();
    if (isRunning) return;
    MockingbirdException.check(_platform.start(_handle), 'mockingbird_start');
    _started(pollInterval);
  }

  /// Stops capture and drains the final events. Closing the microphone
  /// blocks this isolate meanwhile; [stopAsync] does not.
  void stop() {
    _ensureLive();
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    _stopped(_platform.stop(_handle));
  }

  /// [start] with the microphone opened on the engine's own thread, so this
  /// isolate keeps running; completes once capture runs. [startAsync] and
  /// [stopAsync] run in the order they were called; do not mix them with
  /// [start] and [stop] while one is pending.
  Future<void> startAsync({Duration? pollInterval}) async {
    _ensureLive();
    await _queue.add(() async {
      if (isRunning) return;
      final s = await _platform.startAsync(_handle);
      MockingbirdException.check(s, 'mockingbird_start');
      _started(pollInterval);
    });
  }

  /// [stop] with the microphone closed on the engine's own thread; completes
  /// once capture has stopped and the final events are drained.
  Future<void> stopAsync() async {
    _ensureLive();
    await _queue.add(_stopAsync);
  }

  Future<void> _stopAsync() async {
    if (!isRunning) return;
    _timer?.cancel();
    _timer = null;
    _stopped(await _platform.stopAsync(_handle));
  }

  void _started(Duration? pollInterval) {
    final pushed = _platform.setWakeListener(_handle, _wake);
    final every = pollInterval ?? Duration(milliseconds: pushed ? 250 : 50);
    _timer = Timer.periodic(every, (_) => _poll());
    if (pushed) _wake();
  }

  void _stopped(int status) {
    MockingbirdException.check(status, 'mockingbird_stop');
    _poll();
    _platform.setWakeListener(_handle, null);
  }

  void _poll() => _guard(_poller.poll);

  void _wake() {
    if (_disposed || !isRunning) return;
    _platform.ackWake(_handle);
    _guard(_poller.drain);
  }

  void _guard(void Function() body) {
    try {
      body();
      _out.afterDrain();
    } on MockingbirdException catch (e, st) {
      _out.events.addError(e, st);
    }
  }
}
