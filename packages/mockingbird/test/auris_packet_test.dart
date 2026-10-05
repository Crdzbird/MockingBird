import 'package:mockingbird/mockingbird.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  AurisPacket packet(int flags) => AurisPacket(
    segmentId: 1,
    ptsUs: 0,
    captureTsUs: 0,
    flags: flags,
    codec: AurisCodec.opus,
    data: Uint8List(0),
  );

  test('flag getters read the native bits', () {
    expect(packet(1).isPreroll, isTrue);
    expect(packet(2).isPostroll, isTrue);
    expect(packet(4).isKeyframe, isTrue);
    expect(packet(8).isLast, isTrue);
    expect(packet(0).isPreroll, isFalse);
    expect(packet(12).isKeyframe && packet(12).isLast, isTrue);
  });

  test('unknown codecs and statuses fall back safely', () {
    expect(AurisCodec.fromNative(99), AurisCodec.pcm16);
    expect(AurisStatus.fromNative(99), AurisStatus.internal);
    expect(AurisStatus.fromNative(-7), AurisStatus.queueEmpty);
  });
}
