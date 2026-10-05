import 'package:mockingbird/src/auris_backend.dart';
import 'package:mockingbird/src/auris_vad_tier.dart';
import 'package:mockingbird/src/config/auris_anomaly_config.dart';
import 'package:mockingbird/src/config/auris_encoder_config.dart';
import 'package:mockingbird/src/config/auris_performance_config.dart';
import 'package:mockingbird/src/config/auris_queue_config.dart';
import 'package:mockingbird/src/config/auris_segment_config.dart';
import 'package:mockingbird/src/config/auris_sound_class_config.dart';
import 'package:mockingbird/src/config/auris_vad_config.dart';
import 'package:mockingbird/src/config/sensitivity.dart';

/// Everything an `AurisEngine` needs, grouped by concern. The defaults give
/// fast VAD plus the anomaly gate on the platform's microphone.
final class AurisConfig {
  /// Creates a configuration.
  const AurisConfig({
    this.backend = .auto,
    this.sampleRateHz = 16000,
    this.frameMs = 20,
    this.denoise = false,
    this.vad = const AurisVadConfig(),
    this.anomaly = const AurisAnomalyConfig(),
    this.soundClass = const AurisSoundClassConfig(),
    this.segments = const AurisSegmentConfig(),
    this.encoder = const AurisEncoderConfig(),
    this.queues = const AurisQueueConfig(),
    this.performance = const AurisPerformanceConfig(),
  });

  /// Every detector on, driven by one [sensitivity] dial in 0..1, with
  /// neural VAD so voice detection is multilingual.
  factory AurisConfig.sensitive(
    double sensitivity, {
    AurisBackend backend = .auto,
    bool denoise = false,
    bool vad = true,
    AurisVadTier vadTier = .neural,
    String? neuralVadModelPath,
    bool anomaly = true,
    bool soundClass = true,
    String? soundClassModelPath,
    AurisEncoderConfig encoder = const AurisEncoderConfig(),
    AurisPerformanceConfig performance = const AurisPerformanceConfig(),
  }) {
    final s = Sensitivity(sensitivity);
    return AurisConfig(
      backend: backend,
      denoise: denoise,
      vad: AurisVadConfig.sensitive(
        s,
        enabled: vad,
        tier: vadTier,
        modelPath: neuralVadModelPath,
      ),
      anomaly: AurisAnomalyConfig.sensitive(s, enabled: anomaly),
      soundClass: AurisSoundClassConfig.sensitive(
        s,
        enabled: soundClass,
        modelPath: soundClassModelPath,
      ),
      encoder: encoder,
      performance: performance,
    );
  }

  /// Audio source.
  final AurisBackend backend;

  /// Working sample rate: 8000, 16000, 32000 or 48000.
  final int sampleRateHz;

  /// Frame length in milliseconds.
  final int frameMs;

  /// RNNoise noise suppression ahead of every detector and the encoder.
  /// Needs [sampleRateHz] of 8000, 16000 or 48000.
  final bool denoise;

  /// Voice-activity detection.
  final AurisVadConfig vad;

  /// The anomaly gate.
  final AurisAnomalyConfig anomaly;

  /// YAMNet sound classification.
  final AurisSoundClassConfig soundClass;

  /// Segment shaping.
  final AurisSegmentConfig segments;

  /// Packet encoding.
  final AurisEncoderConfig encoder;

  /// Queue sizing and the PCM tap.
  final AurisQueueConfig queues;

  /// CPU and battery trade-offs.
  final AurisPerformanceConfig performance;
}
