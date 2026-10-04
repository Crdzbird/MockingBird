import 'dart:io';

import 'package:harness/src/bench_paths.dart';
import 'package:harness/src/bench_result.dart';
import 'package:harness/src/cpu_clock.dart';
import 'package:harness/src/rss_sampler.dart';

typedef BenchBody = Future<Map<String, num>> Function();

class BenchRunner {
  BenchRunner(this.library, this.paths) {
    Directory(paths.resultsDir).createSync(recursive: true);
    _sink = paths.resultFile(library).openWrite();
  }

  final String library;
  final BenchPaths paths;
  final CpuClock _cpu = CpuClock();
  late final IOSink _sink;

  Future<BenchResult> run(
    String scenario,
    double audioSeconds,
    BenchBody body,
  ) async {
    final rss = RssSampler()..start();
    final cpu0 = _cpu.nowSeconds();
    final wall = Stopwatch()..start();
    final counters = await body();
    wall.stop();
    final cpu1 = _cpu.nowSeconds();
    rss.stop();
    final result = BenchResult(
      library: library,
      scenario: scenario,
      audioSeconds: audioSeconds,
      cpuSeconds: cpu1 - cpu0,
      wallSeconds: wall.elapsedMilliseconds / 1000,
      rssBaseMb: rss.baseMb,
      rssPeakMb: rss.peakMb,
      counters: counters,
    );
    final line = result.toJsonLine();
    stdout.writeln('BENCH $line');
    _sink.writeln(line);
    return result;
  }

  Future<void> close() async {
    await _sink.flush();
    await _sink.close();
  }
}
