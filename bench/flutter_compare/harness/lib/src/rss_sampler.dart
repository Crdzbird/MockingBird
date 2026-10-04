import 'dart:async';
import 'dart:io';

class RssSampler {
  RssSampler() : base = ProcessInfo.currentRss;

  final int base;
  int peak = ProcessInfo.currentRss;
  Timer? _timer;

  void start() {
    _timer = Timer.periodic(const Duration(milliseconds: 50), (_) => sample());
  }

  void sample() {
    final now = ProcessInfo.currentRss;
    if (now > peak) peak = now;
  }

  void stop() {
    _timer?.cancel();
    sample();
  }

  double get baseMb => base / 1048576;

  double get peakMb => peak / 1048576;
}
