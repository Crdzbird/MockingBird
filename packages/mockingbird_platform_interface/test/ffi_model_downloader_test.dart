import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_model_downloader.dart';

import 'src/model_server.dart';

void main() {
  late ModelServer server;
  late Directory dir;
  final body = Uint8List.fromList(
    List.generate(300000, (_) => Random(7).nextInt(256)),
  );
  const downloader = FfiModelDownloader(retryDelay: Duration.zero);

  MockingbirdModelSpec spec(List<String> paths) => MockingbirdModelSpec(
    fileName: 'm.bin',
    bytes: body.length,
    sha256: sha256.convert(body).toString(),
    urls: [for (final p in paths) server.url(p)],
  );

  setUp(() async {
    server = await ModelServer.start(body);
    dir = await Directory.systemTemp.createTemp('mockingbird_models');
  });

  tearDown(() async {
    await server.close();
    await dir.delete(recursive: true);
  });

  test('downloads, verifies and then reuses without the network', () async {
    final path = await downloader.fetch(spec(['/good']), dir.path, null);
    expect(File(path).readAsBytesSync(), body);
    expect(path, endsWith('_m.bin'));
    await downloader.fetch(spec(['/good']), dir.path, null);
    expect(server.requests, ['/good null']);
  });

  test('a dropped connection resumes from where it stopped', () async {
    final seen = <int>[];
    final path = await downloader.fetch(
      spec(['/drop']),
      dir.path,
      (received, total) => seen.add(received),
    );
    expect(File(path).readAsBytesSync(), body);
    expect(server.requests.first, '/drop null');
    expect(server.requests.last, startsWith('/drop bytes='));
    expect(seen.last, body.length);
  });

  test('a source with the wrong bytes or none is skipped', () async {
    final path = await downloader.fetch(
      spec(['/gone', '/bad', '/good']),
      dir.path,
      null,
    );
    expect(File(path).readAsBytesSync(), body);
    expect(server.requests.where((r) => r.startsWith('/gone')), hasLength(3));
    expect(server.requests.where((r) => r.startsWith('/bad')), hasLength(1));
  });

  test('when every source fails the error says which model', () async {
    await expectLater(
      downloader.fetch(spec(['/gone', '/bad']), dir.path, null),
      throwsA(
        isA<MockingbirdDownloadException>().having(
          (e) => e.fileName,
          'fileName',
          'm.bin',
        ),
      ),
    );
    expect(dir.listSync(recursive: true).whereType<File>(), isEmpty);
  });
}
