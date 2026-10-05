import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

void main() {
  MockingbirdPacket packet(int flags) => MockingbirdPacket(
    segmentId: 1,
    ptsUs: 0,
    captureTsUs: 0,
    flags: flags,
    codec: MockingbirdCodec.opus,
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
    expect(MockingbirdCodec.fromNative(99), MockingbirdCodec.pcm16);
    expect(MockingbirdStatus.fromNative(99), MockingbirdStatus.internal);
    expect(MockingbirdStatus.fromNative(-7), MockingbirdStatus.queueEmpty);
  });
}
