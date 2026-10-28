import 'dart:typed_data';

/// Wraps 16-bit little-endian PCM in a RIFF WAVE header.
///
/// Concatenate one segment's `MockingbirdCodec.pcm16` payloads in drain order
/// and pass the bytes here to get a file any player opens.
abstract final class MockingbirdWav {
  /// Size of the header placed ahead of the samples.
  static const int headerBytes = 44;

  /// Returns a complete `.wav` file holding [pcm].
  static Uint8List fromPcm16(
    Uint8List pcm, {
    required int sampleRateHz,
    int channels = 1,
  }) {
    final out = Uint8List(headerBytes + pcm.length);
    ByteData.sublistView(out)
      ..setUint32(4, 36 + pcm.length, Endian.little)
      ..setUint32(16, 16, Endian.little)
      ..setUint16(20, 1, Endian.little)
      ..setUint16(22, channels, Endian.little)
      ..setUint32(24, sampleRateHz, Endian.little)
      ..setUint32(28, sampleRateHz * channels * 2, Endian.little)
      ..setUint16(32, channels * 2, Endian.little)
      ..setUint16(34, 16, Endian.little)
      ..setUint32(40, pcm.length, Endian.little);
    out
      ..setAll(0, 'RIFF'.codeUnits)
      ..setAll(8, 'WAVEfmt '.codeUnits)
      ..setAll(36, 'data'.codeUnits)
      ..setAll(headerBytes, pcm);
    return out;
  }
}
