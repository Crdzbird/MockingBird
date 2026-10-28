import 'dart:typed_data';

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
