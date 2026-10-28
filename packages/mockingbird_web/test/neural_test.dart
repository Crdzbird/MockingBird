@TestOn('browser')
library;

import 'dart:js_interop';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/mockingbird_web.dart';
import 'package:web/web.dart' as web;

import 'test_config.dart';

Future<Uint8List> fetchBytes(String name) async {
  final url = Uri.base.resolve('wasm/$name').toString();
  final response = await web.window.fetch(url.toJS).toDart;
  return (await response.arrayBuffer().toDart).toDart.asUint8List();
}

Int16List wavPcm(Uint8List b) {
  final d = ByteData.sublistView(b);
  for (var off = 12; off + 8 <= b.length;) {
    final size = d.getUint32(off + 4, Endian.little);
    if (String.fromCharCodes(b.sublist(off, off + 4)) == 'data') {
      return Int16List.sublistView(
        Uint8List.fromList(b.sublist(off + 8, off + 8 + size)),
      );
    }
    off += 8 + size + (size & 1);
  }
  throw StateError('no data chunk');
}

void main() {
  test('Silero VAD and YAMNet run in the browser', () async {
    MockingbirdWeb.assetBaseUrl = Uri.base.resolve('wasm/').toString();
    final platform = MockingbirdWeb();
    await platform.initialize();
    final vad = await platform.storeModel(
      'silero_vad.ort',
      await fetchBytes('silero_vad.ort'),
    );
    final yamnet = await platform.storeModel(
      'yamnet_3s.ort',
      await fetchBytes('yamnet_3s.ort'),
    );
    final created = platform.create(
      testConfig(
        detectors: MockingbirdNativeDetectorConfig(
          enableVad: true,
          vadTier: 1,
          vadAggressiveness: 2,
          vadNeuralThreshold: 0.5,
          vadHangoverMs: 300,
          neuralVadModelPath: vad,
          enableAnomaly: false,
          anomalyRmsZ: 3,
          anomalyFluxZ: 3,
          anomalyBackgroundMs: 2000,
          enableSoundClass: true,
          soundClassModelPath: yamnet,
          soundClassThreshold: 0.4,
          soundClassIntervalMs: 1000,
        ),
      ),
    );
    expect(created.status, MockingbirdStatusCode.ok);
    final h = created.handle!;
    expect(platform.start(h), MockingbirdStatusCode.ok);
    final pcm = wavPcm(await fetchBytes('hello.wav'));
    final types = <int>[];
    for (var i = 0, ts = 0; i + 320 <= pcm.length; i += 320, ts += 20000) {
      platform.pushPcm(h, Int16List.sublistView(pcm, i, i + 320), ts);
      for (var e = platform.pollEvent(h).event; e != null;) {
        types.add(e.type);
        e = platform.pollEvent(h).event;
      }
      while (platform.drainPacket(h).packet != null) {}
    }
    expect(platform.stop(h), MockingbirdStatusCode.ok);
    platform.destroy(h);
    expect(types, contains(1));
    expect(types, contains(8));
    expect(types, isNot(contains(7)));
  });
}
