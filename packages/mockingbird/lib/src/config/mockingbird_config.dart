import 'package:mockingbird/src/config/mockingbird_anomaly_config.dart';
import 'package:mockingbird/src/config/mockingbird_encoder_config.dart';
import 'package:mockingbird/src/config/mockingbird_performance_config.dart';
import 'package:mockingbird/src/config/mockingbird_queue_config.dart';
import 'package:mockingbird/src/config/mockingbird_segment_config.dart';
import 'package:mockingbird/src/config/mockingbird_sound_class_config.dart';
import 'package:mockingbird/src/config/mockingbird_vad_config.dart';
import 'package:mockingbird/src/config/mockingbird_voice_processing.dart';
import 'package:mockingbird/src/config/sensitivity.dart';
import 'package:mockingbird/src/mockingbird_backend.dart';
import 'package:mockingbird/src/mockingbird_vad_tier.dart';

part 'mockingbird_config_sensitive.dart';

/// Everything a `MockingbirdEngine` needs, grouped by concern; the defaults
/// run fast VAD and the anomaly gate on the microphone.
final class MockingbirdConfig {
  /// Creates a configuration.
  const factory MockingbirdConfig({
    MockingbirdBackend backend,
    int sampleRateHz,
    int frameMs,
    bool denoise,
    MockingbirdVoiceProcessing voiceProcessing,
    MockingbirdVadConfig vad,
    MockingbirdAnomalyConfig anomaly,
    MockingbirdSoundClassConfig soundClass,
    MockingbirdSegmentConfig segments,
    MockingbirdEncoderConfig encoder,
    MockingbirdQueueConfig queues,
    MockingbirdPerformanceConfig performance,
  }) = MockingbirdConfig._;

  const MockingbirdConfig._({
    this.backend = .auto,
    this.sampleRateHz = 16000,
    this.frameMs = 20,
    this.denoise = false,
    this.voiceProcessing = const MockingbirdVoiceProcessing(),
    this.vad = const MockingbirdVadConfig(),
    this.anomaly = const MockingbirdAnomalyConfig(),
    this.soundClass = const MockingbirdSoundClassConfig(),
    this.segments = const MockingbirdSegmentConfig(),
    this.encoder = const MockingbirdEncoderConfig(),
    this.queues = const MockingbirdQueueConfig(),
    this.performance = const MockingbirdPerformanceConfig(),
  });

  /// Every detector on, driven by one [sensitivity] dial in 0..1, with
  /// neural VAD so voice detection is multilingual.
  factory MockingbirdConfig.sensitive(
    double sensitivity, {
    MockingbirdBackend backend,
    bool denoise,
    bool vad,
    MockingbirdVadTier vadTier,
    String? neuralVadModelPath,
    bool anomaly,
    bool soundClass,
    String? soundClassModelPath,
    MockingbirdEncoderConfig encoder,
    MockingbirdPerformanceConfig performance,
  }) = _SensitiveConfig;

  /// Audio source.
  final MockingbirdBackend backend;

  /// Working sample rate: 8000, 16000, 32000 or 48000.
  final int sampleRateHz;

  /// Frame length in milliseconds.
  final int frameMs;

  /// RNNoise ahead of every detector; needs 8, 16 or 48 kHz.
  final bool denoise;

  /// The platform's echo cancellation, gain control and noise suppression.
  final MockingbirdVoiceProcessing voiceProcessing;

  /// Voice-activity detection.
  final MockingbirdVadConfig vad;

  /// The anomaly gate.
  final MockingbirdAnomalyConfig anomaly;

  /// YAMNet sound classification.
  final MockingbirdSoundClassConfig soundClass;

  /// Segment shaping.
  final MockingbirdSegmentConfig segments;

  /// Packet encoding.
  final MockingbirdEncoderConfig encoder;

  /// Queue sizing and the PCM tap.
  final MockingbirdQueueConfig queues;

  /// CPU and battery trade-offs.
  final MockingbirdPerformanceConfig performance;
}
