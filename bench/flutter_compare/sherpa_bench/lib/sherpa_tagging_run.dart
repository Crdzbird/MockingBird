import 'dart:typed_data';

import 'package:harness/harness.dart';
import 'package:sherpa_onnx/sherpa_onnx.dart' as sherpa;

class SherpaTaggingRun {
  SherpaTaggingRun(this.fixture, this.modelPath, this.labelsPath);

  final Fixture fixture;
  final String modelPath;
  final String labelsPath;

  static const int _window = 3 * Fixture.sampleRate;
  static const int _hop = Fixture.sampleRate;

  Future<Map<String, num>> execute() async {
    final tagger = sherpa.AudioTagging(
      config: sherpa.AudioTaggingConfig(
        model: sherpa.AudioTaggingModelConfig(ced: modelPath, debug: false),
        labels: labelsPath,
      ),
    );
    final all = fixture.toFloat32();
    var inferences = 0;
    var confident = 0;
    for (var end = _window; end <= all.length; end += _hop) {
      final stream = tagger.createStream();
      stream.acceptWaveform(
        samples: Float32List.sublistView(all, end - _window, end),
        sampleRate: Fixture.sampleRate,
      );
      final events = tagger.compute(stream: stream, topK: 1);
      stream.free();
      inferences++;
      if (events.isNotEmpty && events.first.prob >= 0.4) confident++;
      if (inferences % 5 == 0) await Future<void>.delayed(Duration.zero);
    }
    tagger.free();
    return {'inferences': inferences, 'confident': confident};
  }
}
