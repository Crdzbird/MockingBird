import 'package:mockingbird/src/mockingbird_backend.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// How a `MockingbirdPlayer` plays: its output, format and jitter buffer.
final class MockingbirdPlayerConfig {
  /// Creates a config; the defaults suit the engine's 16 kHz mono Opus.
  const factory MockingbirdPlayerConfig({
    MockingbirdBackend backend,
    int sampleRateHz,
    int channels,
    Duration targetDelay,
    Duration maxDelay,
    Duration concealMax,
    int queuePackets,
  }) = MockingbirdPlayerConfig._;

  const MockingbirdPlayerConfig._({
    this.backend = MockingbirdBackend.auto,
    this.sampleRateHz = 16000,
    this.channels = 1,
    this.targetDelay = const Duration(milliseconds: 60),
    this.maxDelay = const Duration(milliseconds: 400),
    this.concealMax = const Duration(milliseconds: 100),
    this.queuePackets = 128,
  });

  /// The output; `hostPcm` opens none and the app pulls with `read`.
  final MockingbirdBackend backend;

  /// 8000, 12000, 16000, 24000 or 48000.
  final int sampleRateHz;

  /// 1 or 2.
  final int channels;

  /// How long the first packet of a talkspurt waits before it plays.
  final Duration targetDelay;

  /// The backlog beyond which the oldest packets are dropped.
  final Duration maxDelay;

  /// The longest hole concealed rather than left silent.
  final Duration concealMax;

  /// Packets that can wait between the app and the audio thread.
  final int queuePackets;

  /// The native form.
  MockingbirdNativePlayerConfig toNative() => MockingbirdNativePlayerConfig(
    backend: backend.nativeValue,
    sampleRateHz: sampleRateHz,
    channels: channels,
    targetDelayMs: targetDelay.inMilliseconds,
    maxDelayMs: maxDelay.inMilliseconds,
    concealMaxMs: concealMax.inMilliseconds,
    queuePackets: queuePackets,
  );
}
