import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

import 'host_audio.dart';

/// The ONNX Runtime detectors and the silence gate in front of YAMNet.
void modelTests() {
  test(
    'the silence gate sheds YAMNet work; worker health is reported',
    () async {
      final model = await MockingbirdModels.soundClassPath();
      final engine = MockingbirdEngine.create(
        hostConfig(
          soundClass: MockingbirdSoundClassConfig(
            enabled: true,
            modelPath: model,
          ),
        ),
      )..start();
      addTearDown(engine.dispose);
      await push(engine, 250, pauseEvery: 25);
      await Future<void>.delayed(const Duration(milliseconds: 500));
      var stats = await engine.stats.first;
      expect(stats.worker.soundClassInferences, 0);
      expect(stats.worker.soundClassInferencesSkipped, greaterThan(0));
      expect(stats.worker.wakeups, greaterThan(0));
      expect(stats.worker.degradationLevel, 0);
      await push(engine, 100, amplitude: 9000, from: 250, pauseEvery: 25);
      await Future<void>.delayed(const Duration(milliseconds: 800));
      stats = await engine.stats.first;
      expect(stats.worker.soundClassInferences, greaterThan(0));
    },
  );

  test('Silero neural VAD loads and runs (ONNX Runtime is linked)', () async {
    final model = await MockingbirdModels.neuralVadPath();
    final engine = MockingbirdEngine.create(
      hostConfig(
        vad: MockingbirdVadConfig(tier: .neural, modelPath: model),
      ),
    )..start();
    addTearDown(engine.dispose);
    await push(engine, 60, amplitude: 12000);
    await Future<void>.delayed(const Duration(milliseconds: 600));
    final stats = await engine.stats.first;
    expect(stats.framesCaptured, 60 * 320);
    expect(engine.isRunning, isTrue);
  });

  test('YAMNet classifies and the event carries a valid class', () async {
    final model = await MockingbirdModels.soundClassPath();
    final engine = MockingbirdEngine.create(
      hostConfig(
        soundClass: MockingbirdSoundClassConfig(
          enabled: true,
          modelPath: model,
        ),
      ),
    )..start();
    addTearDown(engine.dispose);
    final classes = <int>[];
    addTearDown(
      engine.events.listen((e) {
        if (e case MockingbirdSoundClassEvent(:final classId)) {
          classes.add(classId);
        }
      }).cancel,
    );
    await push(engine, 150, amplitude: 9000);
    await Future<void>.delayed(const Duration(seconds: 1));
    expect(classes, isNotEmpty);
    for (final c in classes) {
      expect(c, inInclusiveRange(0, 520));
    }
  });
}
