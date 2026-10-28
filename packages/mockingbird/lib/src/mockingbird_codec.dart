/// How emitted packets are encoded.
enum MockingbirdCodec {
  /// Raw 16-bit little-endian PCM.
  pcm16(0),

  /// Raw Opus packets, the usual choice for live voice.
  opus(1),

  /// FLAC archival encoding (planned).
  flac(2),

  /// Opus in an Ogg container: one segment's payloads, concatenated in
  /// order, form a playable `.opus` file.
  oggOpus(3);

  const MockingbirdCodec(this.nativeValue);

  /// The `mockingbird_codec_t` value.
  final int nativeValue;

  /// Decodes a native codec value; unknown values map to [pcm16].
  static MockingbirdCodec fromNative(int value) =>
      values.firstWhere((c) => c.nativeValue == value, orElse: () => pcm16);
}
