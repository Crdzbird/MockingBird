import 'dart:io';
import 'dart:typed_data';

/// A local HTTP server for download tests. `/good` serves [body] with range
/// support, `/drop` cuts the first response halfway, `/bad` serves other
/// bytes and `/gone` answers 404. [requests] records each path and range.
final class ModelServer {
  ModelServer._(this._server, this.body);

  /// Starts the server on a free local port.
  static Future<ModelServer> start(Uint8List body) async {
    final s = ModelServer._(await HttpServer.bind('127.0.0.1', 0), body);
    s._server.listen(s._serve);
    return s;
  }

  final HttpServer _server;

  /// The file every good path serves.
  final Uint8List body;

  /// Every request as `path range`.
  final List<String> requests = [];

  var _dropped = false;

  /// The URL of [path] on this server.
  Uri url(String path) => Uri.parse('http://127.0.0.1:${_server.port}$path');

  /// Stops the server.
  Future<void> close() => _server.close(force: true);

  Future<void> _serve(HttpRequest r) async {
    final range = r.headers.value(HttpHeaders.rangeHeader);
    requests.add('${r.uri.path} $range');
    final res = r.response;
    if (r.uri.path == '/gone') {
      res.statusCode = HttpStatus.notFound;
      return await res.close();
    }
    if (r.uri.path == '/bad') {
      res.add(Uint8List(body.length));
      return await res.close();
    }
    final from = range == null
        ? 0
        : int.parse(range.split('=')[1].split('-')[0]);
    if (range != null) res.statusCode = HttpStatus.partialContent;
    res.contentLength = body.length - from;
    if (r.uri.path == '/drop' && !_dropped) {
      _dropped = true;
      final socket = await res.detachSocket();
      socket.add(body.sublist(from, body.length ~/ 2));
      await socket.flush();
      return socket.destroy();
    }
    res.add(body.sublist(from));
    return await res.close();
  }
}
