/// A `mockingbird_player_config_t` as plain integers.
final class MockingbirdNativePlayerConfig {
  /// Creates a config; the defaults are the C ABI's.
  const factory MockingbirdNativePlayerConfig({
    int backend,
    int sampleRateHz,
    int channels,
    int targetDelayMs,
    int maxDelayMs,
    int concealMaxMs,
    int queuePackets,
  }) = MockingbirdNativePlayerConfig._;

  const MockingbirdNativePlayerConfig._({
    this.backend = 0,
    this.sampleRateHz = 16000,
    this.channels = 1,
    this.targetDelayMs = 60,
    this.maxDelayMs = 400,
    this.concealMaxMs = 100,
    this.queuePackets = 128,
  });

  /// `mockingbird_backend_t`; 4 (host PCM) opens no device.
  final int backend;

  /// The output rate: 8000, 12000, 16000, 24000 or 48000.
  final int sampleRateHz;

  /// 1 or 2.
  final int channels;

  /// How long the first packet waits before playout.
  final int targetDelayMs;

  /// The backlog beyond which the oldest packets are dropped.
  final int maxDelayMs;

  /// The longest hole that is concealed rather than left silent.
  final int concealMaxMs;

  /// Packet slots between the app and the audio thread.
  final int queuePackets;
}
