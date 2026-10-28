import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

void main() {
  test('wraps PCM in a 44-byte RIFF WAVE header', () {
    final pcm = Uint8List.fromList([1, 0, 2, 0, 3, 0, 4, 0]);
    final wav = MockingbirdWav.fromPcm16(pcm, sampleRateHz: 16000);
    final h = ByteData.sublistView(wav);
    expect(wav.length, 52);
    expect(String.fromCharCodes(wav.sublist(0, 4)), 'RIFF');
    expect(h.getUint32(4, Endian.little), 44);
    expect(String.fromCharCodes(wav.sublist(8, 16)), 'WAVEfmt ');
    expect(h.getUint16(20, Endian.little), 1);
    expect(h.getUint16(22, Endian.little), 1);
    expect(h.getUint32(24, Endian.little), 16000);
    expect(h.getUint32(28, Endian.little), 32000);
    expect(h.getUint16(32, Endian.little), 2);
    expect(h.getUint16(34, Endian.little), 16);
    expect(String.fromCharCodes(wav.sublist(36, 40)), 'data');
    expect(h.getUint32(40, Endian.little), 8);
    expect(wav.sublist(44), pcm);
  });

  test('Ogg Opus maps to native value 3 and back', () {
    expect(MockingbirdCodec.oggOpus.nativeValue, 3);
    expect(MockingbirdCodec.fromNative(3), MockingbirdCodec.oggOpus);
    expect(
      const MockingbirdEncoderConfig.oggOpus().codec,
      MockingbirdCodec.oggOpus,
    );
  });
}
