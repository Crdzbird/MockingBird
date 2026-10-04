import 'package:harness/harness.dart';
import 'package:sherpa_onnx/sherpa_onnx.dart' as sherpa;

class SherpaDenoiseRun {
  SherpaDenoiseRun(this.fixture, this.modelPath);

  final Fixture fixture;
  final String modelPath;

  Future<Map<String, num>> execute() async {
    final denoiser = sherpa.OnlineSpeechDenoiser(
      sherpa.OnlineSpeechDenoiserConfig(
        model: sherpa.OfflineSpeechDenoiserModelConfig(
          gtcrn: sherpa.OfflineSpeechDenoiserGtcrnModelConfig(model: modelPath),
          debug: false,
        ),
      ),
    );
    var frames = 0;
    var outSamples = 0;
    for (final chunk in fixture.floatChunks(320)) {
      final out = denoiser.run(samples: chunk, sampleRate: Fixture.sampleRate);
      outSamples += out.samples.length;
      frames++;
      if (frames % 100 == 0) await Future<void>.delayed(Duration.zero);
    }
    outSamples += denoiser.flush().samples.length;
    denoiser.free();
    return {'frames_pushed': frames, 'samples_out': outSamples};
  }
}
