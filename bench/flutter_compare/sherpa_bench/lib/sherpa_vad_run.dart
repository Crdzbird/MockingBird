import 'package:harness/harness.dart';
import 'package:sherpa_onnx/sherpa_onnx.dart' as sherpa;

class SherpaVadRun {
  SherpaVadRun(this.fixture, this.modelPath);

  final Fixture fixture;
  final String modelPath;

  Future<Map<String, num>> execute() async {
    final vad = sherpa.VoiceActivityDetector(
      config: sherpa.VadModelConfig(
        sileroVad: sherpa.SileroVadModelConfig(
          model: modelPath,
          minSilenceDuration: 0.25,
        ),
        debug: false,
      ),
      bufferSizeInSeconds: 30,
    );
    var frames = 0;
    var segments = 0;
    var speechSamples = 0;
    void drain() {
      while (!vad.isEmpty()) {
        speechSamples += vad.front().samples.length;
        vad.pop();
        segments++;
      }
    }

    for (final chunk in fixture.floatChunks(512)) {
      vad.acceptWaveform(chunk);
      frames++;
      drain();
      if (frames % 64 == 0) await Future<void>.delayed(Duration.zero);
    }
    vad.flush();
    drain();
    vad.free();
    return {
      'frames_pushed': frames,
      'segments': segments,
      'speech_seconds': speechSamples / Fixture.sampleRate,
    };
  }
}
