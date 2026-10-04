import 'package:auris_platform_interface/auris_platform_interface.dart';

/// A 16 kHz host-PCM config with fast VAD and the anomaly gate.
AurisNativeConfig testConfig({int backend = 4, bool denoise = false}) =>
    AurisNativeConfig(
      backend: backend,
      sampleRateHz: 16000,
      frameMs: 20,
      enableRnnoise: denoise,
      detectors: const AurisNativeDetectorConfig(
        enableVad: true,
        vadTier: 0,
        vadAggressiveness: 2,
        vadNeuralThreshold: 0.5,
        vadHangoverMs: 300,
        neuralVadModelPath: null,
        enableAnomaly: true,
        anomalyRmsZ: 3,
        anomalyFluxZ: 3,
        anomalyBackgroundMs: 2000,
        enableSoundClass: false,
        soundClassModelPath: null,
        soundClassThreshold: 0.4,
        soundClassIntervalMs: 1000,
      ),
      output: const AurisNativeOutputConfig(
        preRollMs: 300,
        postRollMs: 300,
        segmentMaxMs: 30000,
        codec: 0,
        opusBitrateBps: 32000,
        opusVbr: true,
        opusComplexity: 5,
        pcmRingFrames: 65536,
        packetQueueCapacity: 512,
        enablePcmTap: false,
        pcmTapFrames: 0,
        silenceGateDbfs: -60,
        enableAdaptiveDegradation: false,
        degradeBusyPpm: 700000,
        workerBatchFrames: 0,
      ),
    );
