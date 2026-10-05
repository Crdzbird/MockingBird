import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_bench/mockingbird_scenario.dart';
import 'package:harness/harness.dart';

class MockingbirdScenarios {
  MockingbirdScenarios(BenchPaths paths)
      : silero = paths.fixture('silero_vad.onnx'),
        yamnet = paths.fixture('yamnet_3s.onnx');

  final String silero;
  final String yamnet;

  static const _queues = MockingbirdQueueConfig(
    pcmRingFrames: 1 << 17,
    packetQueueCapacity: 4096,
  );
  static const _noGate = MockingbirdPerformanceConfig(silenceGateDbfs: -120);
  static const _off = MockingbirdVadConfig(enabled: false);
  static const _anomalyOff = MockingbirdAnomalyConfig(enabled: false);

  List<MockingbirdScenario> get all => [
        MockingbirdScenario(
          'vad_fast',
          const MockingbirdConfig(
            backend: .hostPcm,
            queues: _queues,
            anomaly: _anomalyOff,
          ),
        ),
        MockingbirdScenario(
          'vad_neural',
          MockingbirdConfig(
            backend: .hostPcm,
            queues: _queues,
            vad: MockingbirdVadConfig(tier: .neural, modelPath: silero),
            anomaly: _anomalyOff,
          ),
        ),
        MockingbirdScenario(
          'anomaly',
          const MockingbirdConfig(backend: .hostPcm, queues: _queues, vad: _off),
        ),
        MockingbirdScenario(
          'denoise',
          const MockingbirdConfig(
            backend: .hostPcm,
            queues: _queues,
            denoise: true,
            vad: _off,
            anomaly: _anomalyOff,
            performance: _noGate,
          ),
        ),
        MockingbirdScenario(
          'sound_class',
          MockingbirdConfig(
            backend: .hostPcm,
            queues: _queues,
            vad: _off,
            anomaly: _anomalyOff,
            soundClass: MockingbirdSoundClassConfig(enabled: true, modelPath: yamnet),
            performance: _noGate,
          ),
        ),
        MockingbirdScenario('everything', _everything(const MockingbirdPerformanceConfig())),
        MockingbirdScenario('everything_no_gate', _everything(_noGate)),
      ];

  MockingbirdConfig _everything(MockingbirdPerformanceConfig performance) => MockingbirdConfig(
        backend: .hostPcm,
        queues: _queues,
        denoise: true,
        vad: MockingbirdVadConfig(tier: .neural, modelPath: silero),
        soundClass: MockingbirdSoundClassConfig(enabled: true, modelPath: yamnet),
        encoder: const MockingbirdEncoderConfig.opus(),
        performance: performance,
      );
}
