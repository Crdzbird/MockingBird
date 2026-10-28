import 'dart:typed_data';

import 'package:mockingbird/src/mockingbird_codec.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/mockingbird_wav.dart';

/// One finished segment: every packet the engine emitted for it, in order.
final class MockingbirdSegment {
  /// Creates a segment from its [packets].
  const factory MockingbirdSegment({
    required int id,
    required List<MockingbirdPacket> packets,
    required int sampleRateHz,
    required bool complete,
  }) = MockingbirdSegment._;

  const MockingbirdSegment._({
    required this.id,
    required this.packets,
    required this.sampleRateHz,
    required this.complete,
  });

  /// The segment id the packets and events carry.
  final int id;

  /// The packets, pre-roll first.
  final List<MockingbirdPacket> packets;

  /// The engine's sample rate, which a WAV file needs.
  final int sampleRateHz;

  /// True when the segment's last packet arrived: it closed on its own, or
  /// the engine stopped an Ogg Opus segment and wrote its end-of-stream
  /// page. False when the engine stopped a PCM or raw Opus segment, whose
  /// file is still valid but ends where the audio stopped.
  final bool complete;

  /// The packets' encoding.
  MockingbirdCodec get codec => packets.first.codec;

  /// Capture time of the first packet, microseconds.
  int get startUs => packets.first.captureTsUs;

  /// The extension [toFile] matches.
  String get fileExtension => switch (codec) {
    .pcm16 => 'wav',
    .oggOpus => 'opus',
    _ => 'bin',
  };

  /// The segment as one file: WAV for `pcm16`, Ogg Opus for `oggOpus`.
  /// Raw Opus has no container, so it throws [UnsupportedError]; use
  /// `oggOpus` to get files.
  Uint8List toFile() {
    final joined = BytesBuilder(copy: false);
    for (final p in packets) {
      joined.add(p.data);
    }
    return switch (codec) {
      .pcm16 => MockingbirdWav.fromPcm16(
        joined.takeBytes(),
        sampleRateHz: sampleRateHz,
      ),
      .oggOpus => joined.takeBytes(),
      _ => throw UnsupportedError(
        'A ${codec.name} segment has no file format; encode with oggOpus.',
      ),
    };
  }
}
