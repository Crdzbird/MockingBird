import 'dart:async';

/// Measures the longest gap between ticks of a 1 ms timer while a call
/// runs: how long the call kept this isolate from handling events.
final class StallProbe {
  /// Creates a probe.
  factory StallProbe() = StallProbe._;

  StallProbe._();

  final _clock = Stopwatch();
  Timer? _timer;
  Duration _last = Duration.zero;
  Duration _worst = Duration.zero;

  /// Runs [call] and returns the longest stall and the call's duration.
  Future<({Duration stall, Duration total})> measure(
    Future<void> Function() call,
  ) async {
    _clock
      ..reset()
      ..start();
    _last = Duration.zero;
    _worst = Duration.zero;
    _timer = Timer.periodic(const Duration(milliseconds: 1), (_) => _tick());
    final began = _clock.elapsed;
    await call();
    final total = _clock.elapsed - began;
    await Future<void>.delayed(const Duration(milliseconds: 20));
    _timer?.cancel();
    _timer = null;
    _tick();
    return (stall: _worst, total: total);
  }

  void _tick() {
    final now = _clock.elapsed;
    final gap = now - _last;
    if (gap > _worst) _worst = gap;
    _last = now;
  }
}
