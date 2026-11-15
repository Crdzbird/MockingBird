import 'package:flutter/foundation.dart' show debugPrint, kIsWeb;
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'stall_probe.dart';

const _liveMic = bool.fromEnvironment('MOCKINGBIRD_LIVE_MIC');
const _limit = Duration(milliseconds: 100);

Future<Duration> _timed(String name, Future<void> Function() call) async {
  final r = await StallProbe().measure(call);
  debugPrint(
    'STALL $name stall=${r.stall.inMilliseconds}ms '
    'total=${r.total.inMilliseconds}ms',
  );
  return r.stall;
}

/// How long opening and closing the device output and input keep the
/// calling isolate from handling events.
void lifecycleTimingTests() {
  test('player lifecycle calls leave the isolate free', () async {
    final stalls = <Duration>[];
    for (var run = 0; run < 5; run++) {
      final player = MockingbirdPlayer.create()..muted = true;
      stalls
        ..add(await _timed('player.start', player.start))
        ..add(await _timed('player.pause', player.pause))
        ..add(await _timed('player.resume', player.resume))
        ..add(await _timed('player.stop', player.stop))
        ..add(await _timed('player.dispose', player.dispose));
    }
    expect(stalls.where((s) => s > _limit), isEmpty);
  });

  test('capture startAsync and stopAsync leave the isolate free', () async {
    final engine = MockingbirdEngine.create(
      const MockingbirdConfig(
        vad: MockingbirdVadConfig(enabled: false),
        anomaly: MockingbirdAnomalyConfig(enabled: false),
      ),
    );
    final stalls = <Duration>[];
    for (var run = 0; run < 5; run++) {
      stalls.add(await _timed('engine.startAsync', engine.startAsync));
      await Future<void>.delayed(const Duration(milliseconds: 200));
      stalls.add(await _timed('engine.stopAsync', engine.stopAsync));
    }
    stalls.add(await _timed('engine.dispose', engine.dispose));
    expect(stalls.where((s) => s > _limit), isEmpty);
  }, skip: !_liveMic || kIsWeb);

  test('capture start and stop block the isolate', () async {
    final engine = MockingbirdEngine.create(
      const MockingbirdConfig(
        vad: MockingbirdVadConfig(enabled: false),
        anomaly: MockingbirdAnomalyConfig(enabled: false),
      ),
    );
    for (var run = 0; run < 5; run++) {
      await _timed('engine.start', () async => engine.start());
      await Future<void>.delayed(const Duration(milliseconds: 200));
      await _timed('engine.stop', () async => engine.stop());
    }
    await _timed('engine.dispose', engine.dispose);
  }, skip: !_liveMic || kIsWeb);
}
