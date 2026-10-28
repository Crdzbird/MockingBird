import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'host_audio.dart';

/// Keyword spotting loads the bundled front end and a keyword model.
void keywordTests() {
  test('keyword spotting loads its models and runs', () async {
    final engine = MockingbirdEngine.create(hostConfig())
      ..setKeywordFrontend(
        melspectrogram: await MockingbirdModels.keywordMelspectrogramPath(),
        embedding: await MockingbirdModels.keywordEmbeddingPath(),
      );
    addTearDown(engine.dispose);
    final model = await MockingbirdModels.fromAsset(
      kIsWeb ? 'assets/keyword_test.ort' : 'assets/keyword_test.onnx',
    );
    expect(engine.addKeyword(model, threshold: 0.9), 0);
    final errors = <MockingbirdBackendErrorEvent>[];
    addTearDown(
      engine.events.listen((e) {
        if (e case final MockingbirdBackendErrorEvent error) errors.add(error);
      }).cancel,
    );
    engine.start();
    await push(engine, 150, amplitude: 9000);
    await Future<void>.delayed(const Duration(milliseconds: 600));
    final stats = await engine.stats.first;
    expect(stats.framesCaptured, 150 * 320);
    expect(errors, isEmpty);
  });
}
