import 'dart:math';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:mockingbird/mockingbird.dart';

Int16List frame({int amplitude = 0}) {
  final f = Int16List(320);
  for (var i = 0; i < f.length; i++) {
    f[i] = (i % 32 < 16) ? amplitude : -amplitude;
  }
  return f;
}

MockingbirdConfig hostConfig({
  MockingbirdVadConfig vad = const MockingbirdVadConfig(enabled: false),
  MockingbirdAnomalyConfig anomaly = const MockingbirdAnomalyConfig(
    enabled: false,
  ),
  MockingbirdSoundClassConfig soundClass = const MockingbirdSoundClassConfig(),
  bool tap = false,
}) => MockingbirdConfig(
  backend: .hostPcm,
  vad: vad,
  anomaly: anomaly,
  soundClass: soundClass,
  queues: MockingbirdQueueConfig(
    pcmRingFrames: 1 << 17,
    packetQueueCapacity: 4096,
    pcmTap: tap,
    pcmTapFrames: 1 << 15,
  ),
);

Future<void> push(
  MockingbirdEngine engine,
  int frames, {
  int amplitude = 0,
  int from = 0,
  int pauseEvery = 10,
}) async {
  for (var i = from; i < from + frames; i++) {
    engine.pushPcm(frame(amplitude: amplitude), captureTsUs: i * 20000);
    if (i % pauseEvery == pauseEvery - 1) {
      await Future<void>.delayed(const Duration(milliseconds: 30));
    }
  }
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  test('the native ABI resolves through dart:ffi', () {
    expect(RegExp(r'^\d+\.\d+\.\d+').hasMatch(Mockingbird.version), isTrue);
  });

  test('passthrough emits one packet per pushed frame', () async {
    final engine = MockingbirdEngine.create(hostConfig())..start();
    addTearDown(engine.dispose);
    final packets = <MockingbirdPacket>[];
    addTearDown(engine.packets.listen(packets.add).cancel);
    await push(engine, 10);
    await Future<void>.delayed(const Duration(milliseconds: 400));
    expect(packets.length, 10);
    expect(packets.fold<int>(0, (n, p) => n + p.data.length), 10 * 320 * 2);
  });

  test('stats report the input level; the PCM tap is ungated', () async {
    final engine = MockingbirdEngine.create(hostConfig(tap: true))..start();
    addTearDown(engine.dispose);
    const amp = 8192;
    await push(engine, 10, amplitude: amp);
    await Future<void>.delayed(const Duration(milliseconds: 400));
    final stats = await engine.stats.first;
    expect(stats.inputRmsDbfs, closeTo(20 * log(amp / 32768) / ln10, 0.5));
    var tapped = 0;
    for (var c = engine.readPcm(); c.isNotEmpty; c = engine.readPcm()) {
      tapped += c.length;
    }
    expect(tapped, 10 * 320);
  });

  test('a live retune applies; a structural change is rejected', () async {
    final config = hostConfig(anomaly: const MockingbirdAnomalyConfig());
    final engine = MockingbirdEngine.create(config)..start();
    addTearDown(engine.dispose);
    await push(engine, 5, amplitude: 4000);
    engine.updateConfig(config);
    expect(engine.isRunning, isTrue);
    expect(
      () => engine.updateConfig(const MockingbirdConfig(backend: .hostPcm)),
      throwsA(isA<MockingbirdException>()),
    );
  });

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
