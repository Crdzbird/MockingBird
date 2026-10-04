import 'dart:typed_data';

/// A flat mirror of `auris_packet_t` whose payload has been copied.
final class AurisNativePacket {
  /// Creates a packet record.
  const AurisNativePacket({
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
  /// `auris_packet_flags_t` bits.
  final int flags;

  /// The `auris_codec_t` value as an integer.
  final int codec;

  /// The encoded bytes, owned by the caller.
  final Uint8List data;
}
