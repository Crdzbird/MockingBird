import 'dart:typed_data';

/// A flat mirror of `mockingbird_packet_t` whose payload has been copied.
final class MockingbirdNativePacket {
  /// Creates a packet record.
  const factory MockingbirdNativePacket({
    required int segmentId,
    required int ptsUs,
    required int captureTsUs,
    required int flags,
    required int codec,
    required Uint8List data,
  }) = MockingbirdNativePacket._;

  const MockingbirdNativePacket._({
    required this.segmentId,
    required this.ptsUs,
    required this.captureTsUs,
    required this.flags,
    required this.codec,
    required this.data,
  });

  /// Segment the packet belongs to.
  final int segmentId;

  /// Presentation timestamp in microseconds.
  final int ptsUs;

  /// Capture timestamp of the first sample.
  final int captureTsUs;

  /// `mockingbird_packet_flags_t` bits.
  final int flags;

  /// The `mockingbird_codec_t` value as an integer.
  final int codec;

  /// The encoded bytes, owned by the caller.
  final Uint8List data;
}
