import 'dart:async';
import 'dart:collection';
import 'dart:io';
import 'dart:typed_data';

/// Turns a socket's chunks into reads of exact sizes, in request order.
final class ByteReader {
  /// Buffers everything [s] delivers.
  factory ByteReader(Socket s) = ByteReader._;

  ByteReader._(Socket s) {
    s.listen((d) {
      _buf.addAll(d);
      _pump();
    });
  }

  final Queue<int> _buf = Queue<int>();
  final Queue<({int n, Completer<Uint8List> done})> _reqs = Queue();

  /// Completes with the next [n] bytes.
  Future<Uint8List> read(int n) {
    final c = Completer<Uint8List>();
    _reqs.add((n: n, done: c));
    _pump();
    return c.future;
  }

  void _pump() {
    while (_reqs.isNotEmpty && _buf.length >= _reqs.first.n) {
      final req = _reqs.removeFirst();
      final out = Uint8List(req.n);
      for (var i = 0; i < req.n; i++) {
        out[i] = _buf.removeFirst();
      }
      req.done.complete(out);
    }
  }
}
