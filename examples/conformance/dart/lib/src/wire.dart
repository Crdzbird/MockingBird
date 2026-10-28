import 'dart:io';
import 'dart:typed_data';

import 'package:mockingbird_conformance/src/byte_reader.dart';

/// Sends [payload] with a four-byte big-endian length in front.
void writeFrame(Socket s, Uint8List payload) {
  s
    ..add((ByteData(4)..setUint32(0, payload.length)).buffer.asUint8List())
    ..add(payload);
}

/// Reads one length-prefixed frame.
Future<Uint8List> readFrame(ByteReader r) async {
  final hdr = await r.read(4);
  return r.read(hdr.buffer.asByteData().getUint32(0));
}

/// [s] as little-endian bytes.
Uint8List pcmToBytes(Int16List s) {
  final bd = ByteData(s.length * 2);
  for (var i = 0; i < s.length; i++) {
    bd.setInt16(i * 2, s[i], Endian.little);
  }
  return bd.buffer.asUint8List();
}

/// Little-endian bytes back to samples.
Int16List bytesToPcm(Uint8List b) {
  final out = Int16List(b.length ~/ 2);
  final bd = b.buffer.asByteData(b.offsetInBytes);
  for (var i = 0; i < out.length; i++) {
    out[i] = bd.getInt16(i * 2, Endian.little);
  }
  return out;
}

/// The result frame both peers exchange.
Uint8List encodeResult(int computed, int expected, int nPkts, int nEv, int n) {
  final bd = ByteData(28)
    ..setUint64(0, computed)
    ..setUint64(8, expected)
    ..setUint32(16, nPkts)
    ..setUint32(20, nEv)
    ..setUint32(24, n);
  return bd.buffer.asUint8List();
}

/// True when [a] and [b] hold the same bytes.
bool bytesEqual(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}
