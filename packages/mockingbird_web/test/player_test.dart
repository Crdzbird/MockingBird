@TestOn('browser')
library;

import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/mockingbird_web.dart';

import 'opus_packets.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late MockingbirdWeb platform;

  setUpAll(() async {
    MockingbirdWeb.assetBaseUrl = Uri.base.resolve('wasm/').toString();
    platform = MockingbirdWeb();
    MockingbirdPlatform.instance = platform;
    await platform.initialize();
  });

  test('the decoder turns engine packets back into frames', () {
    final packets = opusPackets(platform);
    expect(packets, hasLength(25));
    final d = platform.createDecoder(16000, 1).handle!;
    for (final p in packets) {
      expect(platform.decoderDecode(d, p).samples, hasLength(320));
    }
    expect(platform.decoderConceal(d, 0).samples, hasLength(320));
    final bad = platform.decoderDecode(d, Uint8List.fromList([255, 255, 255]));
    expect(bad.status, MockingbirdStatusCode.invalidArg);
    platform.destroyDecoder(d);
  });

  test('a host-PCM player plays pushed packets in order', () async {
    final packets = opusPackets(platform);
    const config = MockingbirdNativePlayerConfig(backend: 4, maxDelayMs: 2000);
    final h = platform.createPlayer(config).handle!;
    expect(await platform.playerStart(h), MockingbirdStatusCode.ok);
    for (var i = 0; i < packets.length; i++) {
      final flags = i == packets.length - 1 ? 8 : 0;
      final s = platform.playerPush(
        h,
        codec: 1,
        data: packets[i],
        ptsUs: i * 20000,
        flags: flags,
      );
      expect(s, MockingbirdStatusCode.ok);
    }
    final out = platform.playerRead(h, 8000);
    expect(out.samples, hasLength(8000));
    expect(out.samples.any((v) => v != 0), isTrue);
    final stats = platform.playerStats(h).stats!;
    expect(stats.packetsPlayed, 25);
    expect(stats.state, 1);
    await platform.destroyPlayer(h);
  });

  test('a device player schedules its output through Web Audio', () async {
    const config = MockingbirdNativePlayerConfig(targetDelayMs: 0);
    final h = platform.createPlayer(config).handle!;
    platform.playerSetMuted(h, muted: true);
    expect(await platform.playerStart(h), MockingbirdStatusCode.ok);
    await Future<void>.delayed(const Duration(milliseconds: 300));
    expect(platform.playerStats(h).stats!.framesRendered, greaterThan(1600));
    expect(await platform.playerPause(h), MockingbirdStatusCode.ok);
    expect(await platform.playerResume(h), MockingbirdStatusCode.ok);
    expect(await platform.playerStop(h), MockingbirdStatusCode.ok);
    await platform.destroyPlayer(h);
  });
}
