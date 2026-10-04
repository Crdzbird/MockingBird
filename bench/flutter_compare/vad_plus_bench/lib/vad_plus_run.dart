import 'package:harness/harness.dart';
import 'package:vad_plus/vad_plus.dart';

class VadPlusRun {
  VadPlusRun(this.fixture);

  final Fixture fixture;

  Future<Map<String, num>> execute() async {
    final vad = VadPlus();
    await vad.initialize(
      config: const VadConfig(sampleRate: 16000, frameSamples: 512),
    );
    var frames = 0;
    var starts = 0;
    var ends = 0;
    final subscription = vad.events.listen((event) {
      switch (event) {
        case VadFrameProcessed():
          frames++;
        case VadSpeechStart():
          starts++;
        case VadSpeechEnd():
          ends++;
        default:
          break;
      }
    });
    var pushed = 0;
    for (final chunk in fixture.floatChunks(512)) {
      vad.processAudio(chunk);
      pushed++;
      if (pushed % 32 == 0) await Future<void>.delayed(Duration.zero);
    }
    final deadline = DateTime.now().add(const Duration(seconds: 30));
    while (frames < pushed && DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 20));
    }
    await subscription.cancel();
    vad.dispose();
    return {
      'frames_pushed': pushed,
      'frames_processed': frames,
      'speech_start': starts,
      'speech_end': ends,
    };
  }
}
