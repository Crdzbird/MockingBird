import 'dart:typed_data';

/// [v] as sixteen unsigned hex digits.
String hex16(int v) =>
    '0x${BigInt.from(v).toUnsigned(64).toRadixString(16).padLeft(16, '0')}';

/// The word a parity check prints.
String ok(bool b) => b ? 'MATCH' : 'MISMATCH';

/// The bits of [d] as a float32.
int float32bits(double d) =>
    (ByteData(4)..setFloat32(0, d, Endian.little)).getUint32(0, Endian.little);
