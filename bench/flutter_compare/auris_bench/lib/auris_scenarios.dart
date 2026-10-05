import 'package:mockingbird/mockingbird.dart';
import 'package:auris_bench/auris_scenario.dart';
import 'package:harness/harness.dart';

class AurisScenarios {
  AurisScenarios(BenchPaths paths)
      : silero = paths.fixture('silero_vad.onnx'),
        yamnet = paths.fixture('yamnet_3s.onnx');

  final String silero;
  final String yamnet;

  static const _queues = AurisQueueConfig(
    pcmRingFrames: 1 << 17,
    packetQueueCapacity: 4096,
  );
  static const _noGate = AurisPerformanceConfig(silenceGateDbfs: -120);
  static const _off = AurisVadConfig(enabled: false);
  static const _anomalyOff = AurisAnomalyConfig(enabled: false);

  List<AurisScenario> get all => [
        AurisScenario(
          'vad_fast',
          const AurisConfig(
            backend: .hostPcm,
            queues: _queues,
            anomaly: _anomalyOff,
          ),
        ),
        AurisScenario(
          'vad_neural',
          AurisConfig(
            backend: .hostPcm,
            queues: _queues,
            vad: AurisVadConfig(tier: .neural, modelPath: silero),
            anomaly: _anomalyOff,
          ),
        ),
        AurisScenario(
          'anomaly',
          const AurisConfig(backend: .hostPcm, queues: _queues, vad: _off),
        ),
        AurisScenario(
          'denoise',
          const AurisConfig(
            backend: .hostPcm,
            queues: _queues,
            denoise: true,
            vad: _off,
            anomaly: _anomalyOff,
            performance: _noGate,
          ),
        ),
        AurisScenario(
          'sound_class',
          AurisConfig(
            backend: .hostPcm,
            queues: _queues,
            vad: _off,
            anomaly: _anomalyOff,
            soundClass: AurisSoundClassConfig(enabled: true, modelPath: yamnet),
            performance: _noGate,
          ),
        ),
        AurisScenario('everything', _everything(const AurisPerformanceConfig())),
        AurisScenario('everything_no_gate', _everything(_noGate)),
      ];

  AurisConfig _everything(AurisPerformanceConfig performance) => AurisConfig(
        backend: .hostPcm,
        queues: _queues,
        denoise: true,
        vad: AurisVadConfig(tier: .neural, modelPath: silero),
        soundClass: AurisSoundClassConfig(enabled: true, modelPath: yamnet),
        encoder: const AurisEncoderConfig.opus(),
        performance: performance,
      );
}
