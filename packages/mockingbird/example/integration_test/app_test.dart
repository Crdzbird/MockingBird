import 'package:integration_test/integration_test.dart';

import 'src/capture_tests.dart';
import 'src/download_tests.dart';
import 'src/engine_tests.dart';
import 'src/keyword_tests.dart';
import 'src/lifecycle_timing_tests.dart';
import 'src/model_tests.dart';
import 'src/player_tests.dart';
import 'src/speaker_tests.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  engineTests();
  downloadTests();
  modelTests();
  keywordTests();
  speakerTests();
  captureTests();
  playerTests();
  lifecycleTimingTests();
}
