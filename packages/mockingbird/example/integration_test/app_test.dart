import 'package:integration_test/integration_test.dart';

import 'src/engine_tests.dart';
import 'src/model_tests.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  engineTests();
  modelTests();
}
