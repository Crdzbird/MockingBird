@TestOn('browser')
library;

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/mockingbird_web.dart';

import 'test_config.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late MockingbirdWeb platform;

  setUpAll(() async {
    MockingbirdWeb.assetBaseUrl = Uri.base.resolve('wasm/').toString();
    platform = MockingbirdWeb();
    await platform.initialize();
  });

  test('Opus and Ogg Opus at 30 ms frames are unsupported', () {
    for (final codec in [1, 3]) {
      final created = platform.create(testConfig(codec: codec, frameMs: 30));
      expect(created.status, MockingbirdStatusCode.unsupported);
      expect(created.handle, isNull);
    }
  });

  test('PCM keeps 30 ms frames', () {
    final created = platform.create(testConfig(frameMs: 30));
    expect(created.status, MockingbirdStatusCode.ok);
    platform.destroy(created.handle!);
  });
}
