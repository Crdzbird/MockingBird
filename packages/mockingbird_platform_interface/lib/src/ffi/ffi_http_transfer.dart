import 'dart:io';

import 'package:mockingbird_platform_interface/src/mockingbird_download_progress.dart';

/// One attempt at fetching a file into a partial file on disk. It resumes
/// from the bytes already there when the server honours ranges, and gives up
/// when the server stalls.
final class FfiHttpTransfer {
  /// Creates the transfer with its [stallTimeout].
  const factory FfiHttpTransfer({Duration stallTimeout}) = FfiHttpTransfer._;

  const FfiHttpTransfer._({this.stallTimeout = const Duration(seconds: 30)});

  /// How long the connection may stay silent.
  final Duration stallTimeout;

  /// Fetches [url] into [part] until it holds [total] bytes.
  Future<void> fetch(
    Uri url,
    File part,
    int total,
    MockingbirdDownloadProgress? onProgress,
  ) async {
    final have = part.existsSync() ? part.lengthSync() : 0;
    if (have == total) return;
    final client = HttpClient()..connectionTimeout = stallTimeout;
    try {
      final request = await client.getUrl(url);
      if (have > 0 && have < total) {
        request.headers.set(HttpHeaders.rangeHeader, 'bytes=$have-');
      }
      final response = await request.close().timeout(stallTimeout);
      final status = response.statusCode;
      if (status != HttpStatus.ok && status != HttpStatus.partialContent) {
        throw HttpException('HTTP $status', uri: url);
      }
      final resume = status == HttpStatus.partialContent && have < total;
      final sink = part.openWrite(
        mode: resume ? FileMode.append : FileMode.write,
      );
      var received = resume ? have : 0;
      try {
        await for (final chunk in response.timeout(stallTimeout)) {
          received += chunk.length;
          if (received > total) throw HttpException('too long', uri: url);
          sink.add(chunk);
          onProgress?.call(received, total);
        }
      } finally {
        await sink.close();
      }
    } finally {
      client.close(force: true);
    }
  }
}
