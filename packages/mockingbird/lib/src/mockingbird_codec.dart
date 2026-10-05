/// How emitted packets are encoded.
enum MockingbirdCodec {
  /// Raw 16-bit little-endian PCM.
  pcm16(0),

  /// Opus, the default for live voice.
  opus(1),

  /// FLAC archival encoding (planned).
  flac(2);

  const MockingbirdCodec(this.nativeValue);

  /// The `mockingbird_codec_t` value.
  final int nativeValue;

  /// Decodes a native codec value; unknown values map to [pcm16].
  static MockingbirdCodec fromNative(int value) =>
      values.firstWhere((c) => c.nativeValue == value, orElse: () => pcm16);
}
