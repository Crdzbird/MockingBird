import 'dart:typed_data';

import 'package:mockingbird/src/mockingbird_codec.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// One encoded audio packet. [data] is a Dart-owned copy, safe to keep.
final class MockingbirdPacket {
  /// Creates a packet.
  const MockingbirdPacket({
    required this.segmentId,
    required this.ptsUs,
    required this.captureTsUs,
    required this.flags,
    required this.codec,
    required this.data,
  });

  /// Copies a native `mockingbird_packet_t` out of the engine's pool.
  factory MockingbirdPacket.fromNative(MockingbirdNativePacket p) =>
      MockingbirdPacket(
        segmentId: p.segmentId,
        ptsUs: p.ptsUs,
        captureTsUs: p.captureTsUs,
        flags: p.flags,
        codec: MockingbirdCodec.fromNative(p.codec),
        data: p.data,
      );

  /// The segment this packet belongs to; monotonic per engine.
  final int segmentId;

  /// Offset from the segment's start, microseconds.
  final int ptsUs;

  /// Capture-clock timestamp, microseconds.
  final int captureTsUs;

  /// Raw flag bits; prefer the `is*` getters.
  final int flags;

  /// The packet's encoding.
  final MockingbirdCodec codec;

  /// The encoded bytes.
  final Uint8List data;

  /// Captured before the trigger, from the pre-roll buffer.
  bool get isPreroll => flags & 1 != 0;

  /// Trailing audio after the detector closed.
  bool get isPostroll => flags & 2 != 0;

  /// First encoded frame of its segment.
  bool get isKeyframe => flags & 4 != 0;

  /// Terminal packet of its segment.
  bool get isLast => flags & 8 != 0;

  @override
  String toString() =>
      'MockingbirdPacket(segment=$segmentId pts=$ptsUs ${codec.name} '
      '${data.length}B)';
}
