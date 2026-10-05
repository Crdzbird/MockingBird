import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

void main() {
  group('Sensitivity', () {
    test('clamps into 0..1', () {
      expect(Sensitivity(5).value, 1);
      expect(Sensitivity(-5).value, 0);
      expect(Sensitivity(0.25).value, 0.25);
    });

    test('lerps from the least to the most sensitive value', () {
      expect(Sensitivity(0).lerp(0.8, 0.2), closeTo(0.8, 1e-9));
      expect(Sensitivity(1).lerp(0.8, 0.2), closeTo(0.2, 1e-9));
      expect(Sensitivity(0.5).lerp(0.8, 0.2), closeTo(0.5, 1e-9));
    });
  });

  group('MockingbirdConfig.sensitive', () {
    test('lower sensitivity means stricter thresholds everywhere', () {
      final low = MockingbirdConfig.sensitive(0);
      final high = MockingbirdConfig.sensitive(1);
      expect(low.vad.neuralThreshold, greaterThan(high.vad.neuralThreshold));
      expect(low.anomaly.rmsZ, greaterThan(high.anomaly.rmsZ));
      expect(low.anomaly.fluxZ, greaterThan(high.anomaly.fluxZ));
      expect(low.soundClass.threshold, greaterThan(high.soundClass.threshold));
    });

    test('endpoints land on the documented ranges', () {
      final low = MockingbirdConfig.sensitive(0);
      final high = MockingbirdConfig.sensitive(1);
      expect(low.vad.neuralThreshold, closeTo(0.80, 1e-9));
      expect(high.vad.neuralThreshold, closeTo(0.20, 1e-9));
      expect(low.vad.aggressiveness, 3);
      expect(high.vad.aggressiveness, 0);
      expect(low.soundClass.threshold, closeTo(0.70, 1e-9));
      expect(high.soundClass.threshold, closeTo(0.12, 1e-9));
    });

    test('enables every detector with neural VAD by default', () {
      final c = MockingbirdConfig.sensitive(0.5);
      expect(c.vad.enabled, isTrue);
      expect(c.vad.tier, MockingbirdVadTier.neural);
      expect(c.anomaly.enabled, isTrue);
      expect(c.soundClass.enabled, isTrue);
    });
  });

  group('defaults', () {
    test('match the native engine defaults', () {
      const c = MockingbirdConfig();
      expect(c.backend, MockingbirdBackend.auto);
      expect(c.sampleRateHz, 16000);
      expect(c.vad.tier, MockingbirdVadTier.fast);
      expect(c.soundClass.enabled, isFalse);
      expect(c.soundClass.intervalMs, 1000);
      expect(c.performance.silenceGateDbfs, -60);
      expect(c.performance.adaptiveDegradation, isFalse);
      expect(c.performance.workerBatchFrames, 0);
    });

    test('the mobile performance preset sheds work and batches wakeups', () {
      const p = MockingbirdPerformanceConfig.mobile();
      expect(p.adaptiveDegradation, isTrue);
      expect(p.workerBatchFrames, 4);
    });
  });
}
