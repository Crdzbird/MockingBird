import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird/src/native/segment_assembler.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

MockingbirdPacket pkt(
  int segment,
  int flags, {
  MockingbirdCodec codec = .pcm16,
  int fill = 1,
}) => MockingbirdPacket(
  segmentId: segment,
  ptsUs: 0,
  captureTsUs: 1000 * segment,
  flags: flags,
  codec: codec,
  data: Uint8List(4)..fillRange(0, 4, fill),
);

MockingbirdEvent segmentEnd(int segment) => MockingbirdEvent.fromNative(
  MockingbirdNativeEvent(
    type: 5,
    segmentId: segment,
    timestampUs: 0,
    reasonMask: 0,
    confidence: 0,
  ),
);

void main() {
  late List<MockingbirdSegment> out;
  late SegmentAssembler a;
  setUp(() {
    out = [];
    a = SegmentAssembler(sampleRateHz: 16000, onSegment: out.add);
  });

  test('a segment ends on its last packet and becomes a WAV file', () {
    a
      ..packet(pkt(1, 4))
      ..packet(pkt(1, 0, fill: 2))
      ..packet(pkt(1, 8, fill: 3));
    expect(out, hasLength(1));
    final s = out.single;
    expect(
      (s.id, s.complete, s.packets.length, s.fileExtension),
      (1, true, 3, 'wav'),
    );
    final wav = s.toFile();
    expect(wav.length, MockingbirdWav.headerBytes + 12);
    expect(wav.sublist(44), [1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3]);
  });

  test('a segment cut by stop is handed over after the drain', () {
    a
      ..packet(pkt(2, 4))
      ..event(segmentEnd(2))
      ..packet(pkt(2, 0));
    expect(out, isEmpty);
    a.flush();
    expect(out.single.complete, isFalse);
    expect(out.single.packets, hasLength(2));
  });

  test('a segment already under way when listening began is skipped', () {
    a
      ..packet(pkt(3, 0))
      ..packet(pkt(3, 8))
      ..packet(pkt(4, 4 | 8));
    expect(out.map((s) => s.id), [4]);
  });

  test('Ogg Opus segments are files as they are; raw Opus is not', () {
    a
      ..packet(pkt(5, 4 | 8, codec: .oggOpus))
      ..packet(pkt(6, 4 | 8, codec: .opus));
    expect(out[0].toFile(), out[0].packets.single.data);
    expect(out[0].fileExtension, 'opus');
    expect(out[1].toFile, throwsUnsupportedError);
  });
}
