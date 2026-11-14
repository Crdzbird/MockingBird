import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';

/// First use downloads YAMNet (5 MB) and the speaker model (8.4 MB) over the
/// network, which on a fresh device or simulator can take longer than the
/// default 30 s; later calls return the kept files at once, so the model and
/// speaker tests only time the engine.
void downloadTests() {
  test('YAMNet and the speaker model download on first use', () async {
    expect(await MockingbirdModels.soundClassPath(), isNotEmpty);
    expect(await MockingbirdModels.speakerModelPath(), isNotEmpty);
  }, timeout: const Timeout(Duration(minutes: 5)));
}
