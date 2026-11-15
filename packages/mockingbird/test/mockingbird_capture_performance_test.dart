import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird/src/native/native_config_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/zero_stats.dart';

void main() {
  int nativeOf(MockingbirdPerformanceConfig performance) =>
      const NativeConfigMapper()
          .map(MockingbirdConfig(performance: performance))
          .output
          .capturePerformance;

  test('capture performance defaults to normal and maps to the C enum', () {
    expect(
      const MockingbirdPerformanceConfig().capture,
      MockingbirdCapturePerformance.normal,
    );
    expect(
      const MockingbirdPerformanceConfig.mobile().capture,
      MockingbirdCapturePerformance.normal,
    );
    expect(nativeOf(const MockingbirdPerformanceConfig()), 0);
    expect(
      nativeOf(const MockingbirdPerformanceConfig(capture: .lowLatency)),
      1,
    );
    expect(
      nativeOf(const MockingbirdPerformanceConfig(capture: .powerSaving)),
      2,
    );
  });

  test('the ring holds two seconds at 16 kHz by default', () {
    expect(const MockingbirdQueueConfig().pcmRingFrames, 32768);
  });

  test('stats report the capture gaps', () {
    const native = MockingbirdNativeStats(
      framesCaptured: 0,
      framesDroppedCapture: 0,
      packetsEmitted: 0,
      packetsDropped: 0,
      segmentsOpened: 0,
      segmentsClosed: 0,
      pcmRingFillFrames: 0,
      packetQueueFill: 0,
      inputRmsDbfs: 0,
      inputPeakDbfs: 0,
      tapSamplesDropped: 0,
      worker: MockingbirdNativeWorkerStats(
        busyPpm: 0,
        maxFrameUs: 0,
        wakeups: 0,
        soundClassInferences: 0,
        soundClassInferencesSkipped: 0,
        preprocFramesSkipped: 0,
        degradationLevel: 0,
      ),
      voiceProcessingActive: 0,
      captureRestarts: 0,
      captureGaps: 3,
      captureGapMs: 1250,
    );
    final capture = MockingbirdStats.fromNative(native).capture;
    expect(capture.gaps, 3);
    expect(capture.gapTime, const Duration(milliseconds: 1250));
    expect(MockingbirdStats.fromNative(zeroStats).capture.gaps, 0);
  });
}
