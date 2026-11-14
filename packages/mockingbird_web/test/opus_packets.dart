import 'dart:math';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/mockingbird_web.dart';

import 'test_config.dart';

/// Opus packets the engine encodes from half a second of a tone.
List<Uint8List> opusPackets(MockingbirdWeb platform) {
  final config = testConfig(
    codec: 1,
    detectors: const MockingbirdNativeDetectorConfig(
      enableVad: false,
      vadTier: 0,
      vadAggressiveness: 2,
      vadNeuralThreshold: 0.5,
      vadHangoverMs: 300,
      neuralVadModelPath: null,
      enableAnomaly: false,
      anomalyRmsZ: 3,
      anomalyFluxZ: 3,
      anomalyBackgroundMs: 2000,
      enableSoundClass: false,
      soundClassModelPath: null,
      soundClassThreshold: 0.4,
      soundClassIntervalMs: 1000,
    ),
  );
  final h = platform.create(config).handle!;
  platform.start(h);
  final packets = <Uint8List>[];
  final tone = Int16List.fromList(
    List.generate(320, (i) => (6000 * sin(i * 0.1728)).round()),
  );
  for (var i = 0; i < 25; i++) {
    platform.pushPcm(h, tone, i * 20000);
    for (var p = platform.drainPacket(h).packet; p != null;) {
      packets.add(p.data);
      p = platform.drainPacket(h).packet;
    }
  }
  platform.destroy(h);
  return packets;
}
