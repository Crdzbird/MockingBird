@TestOn('vm')
library;

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

import 'support/native_test_platform.dart';

void main() {
  setUpAll(() => MockingbirdPlatform.instance = NativeTestPlatform());

  for (final encoder in const [
    MockingbirdEncoderConfig.opus(),
    MockingbirdEncoderConfig.oggOpus(),
  ]) {
    test('${encoder.codec.name} at 30 ms frames throws unsupported', () {
      expect(
        () => MockingbirdEngine.create(
          MockingbirdConfig(backend: .hostPcm, frameMs: 30, encoder: encoder),
        ),
        throwsA(
          isA<MockingbirdException>().having(
            (e) => e.status,
            'status',
            MockingbirdStatus.unsupported,
          ),
        ),
      );
    }, skip: NativeTestPlatform.available ? false : 'build the library first');
  }

  test('pcm16 keeps 30 ms frames', () async {
    final engine = MockingbirdEngine.create(
      const MockingbirdConfig(backend: .hostPcm, frameMs: 30),
    );
    await engine.dispose();
  }, skip: NativeTestPlatform.available ? false : 'build the library first');
}
