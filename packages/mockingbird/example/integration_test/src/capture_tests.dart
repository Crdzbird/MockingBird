import 'package:flutter/foundation.dart'
    show TargetPlatform, defaultTargetPlatform, kIsWeb;
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

const _liveMic = bool.fromEnvironment('MOCKINGBIRD_LIVE_MIC');

bool get _android => !kIsWeb && defaultTargetPlatform == TargetPlatform.android;

/// Device listing everywhere, and, with
/// `--dart-define=MOCKINGBIRD_LIVE_MIC=true`
/// on a device whose microphone the app may use, live capture with platform
/// voice processing that starts again after a stop.
void captureTests() {
  test('the platform lists its microphones', () async {
    if (_android) {
      await expectLater(
        MockingbirdDevices.input(),
        throwsA(isA<MockingbirdException>()),
      );
      return;
    }
    final devices = await MockingbirdDevices.input();
    for (final d in devices) {
      expect(d.id, isNotEmpty);
    }
    expect(devices.where((d) => d.isDefault).length, lessThanOrEqualTo(1));
  });

  test('live capture applies voice processing and starts again', () async {
    final engine = MockingbirdEngine.create(
      const MockingbirdConfig(
        voiceProcessing: .all(),
        vad: MockingbirdVadConfig(enabled: false),
        anomaly: MockingbirdAnomalyConfig(enabled: false),
      ),
    );
    addTearDown(engine.dispose);
    for (var run = 0; run < 2; run++) {
      engine.start();
      await Future<void>.delayed(const Duration(milliseconds: 700));
      final stats = await engine.stats.first;
      expect(stats.framesCaptured, greaterThan(0));
      expect(stats.capture.voiceProcessing.echoCancellation, isTrue);
      engine.stop();
    }
  }, skip: !_liveMic || kIsWeb);
}
