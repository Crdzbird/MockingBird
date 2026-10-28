import 'dart:typed_data';

/// FNV-1a over 64 bits, fed big-endian integers, as every peer does.
final class Hasher {
  /// Starts from the FNV offset basis.
  factory Hasher() = Hasher._;

  Hasher._();

  static const int _prime = 0x100000001b3;

  /// The running digest.
  int h = 0xcbf29ce484222325;

  /// Mixes one byte.
  void byteVal(int b) {
    h ^= b & 0xFF;
    h *= _prime;
  }

  /// Mixes every byte of [p].
  void bytes(Uint8List p) => p.forEach(byteVal);

  /// Mixes [v] as four big-endian bytes.
  void u32(int v) {
    for (var s = 24; s >= 0; s -= 8) {
      byteVal((v >>> s) & 0xFF);
    }
  }

  /// Mixes [v] as eight big-endian bytes.
  void u64(int v) {
    for (var s = 56; s >= 0; s -= 8) {
      byteVal((v >>> s) & 0xFF);
    }
  }
}
