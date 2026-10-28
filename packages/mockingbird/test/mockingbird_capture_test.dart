import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird/src/native/native_config_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

void main() {
  test('voice processing maps to the native mask and back', () {
    expect(const MockingbirdVoiceProcessing().nativeValue, 0);
    expect(const MockingbirdVoiceProcessing.all().nativeValue, 7);
    expect(
      const MockingbirdVoiceProcessing(echoCancellation: true).nativeValue,
      1,
    );
    expect(
      MockingbirdVoiceProcessing.fromNative(5),
      const MockingbirdVoiceProcessing(
        echoCancellation: true,
        noiseSuppression: true,
      ),
    );
    final native = const NativeConfigMapper().map(
      const MockingbirdConfig(voiceProcessing: .all()),
    );
    expect(native.voiceProcessing, 7);
  });

  test('stats report what capture applied and how often it restarted', () {
    final stats = MockingbirdStats.fromNative(
      const MockingbirdNativeStats(
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
          wakeups: 3,
          soundClassInferences: 0,
          soundClassInferencesSkipped: 0,
          preprocFramesSkipped: 0,
          degradationLevel: 0,
        ),
        voiceProcessingActive: 3,
        captureRestarts: 2,
      ),
    );
    expect(stats.worker.wakeups, 3);
    expect(stats.capture.restarts, 2);
    expect(stats.capture.voiceProcessing.gainControl, isTrue);
    expect(stats.capture.voiceProcessing.noiseSuppression, isFalse);
  });
}
