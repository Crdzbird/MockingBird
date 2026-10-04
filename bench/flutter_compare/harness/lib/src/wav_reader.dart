import 'dart:io';
import 'dart:typed_data';

class WavReader {
  const WavReader();

  Int16List read(String path) {
    final bytes = File(path).readAsBytesSync();
    final data = ByteData.sublistView(bytes);
    var offset = 12;
    while (offset + 8 <= bytes.length) {
      final id = String.fromCharCodes(bytes.sublist(offset, offset + 4));
      final size = data.getUint32(offset + 4, Endian.little);
      if (id == 'data') {
        return Int16List.sublistView(
          Uint8List.sublistView(bytes, offset + 8, offset + 8 + size),
        );
      }
      offset += 8 + size + (size & 1);
    }
    throw FormatException('no data chunk in $path');
  }
}
