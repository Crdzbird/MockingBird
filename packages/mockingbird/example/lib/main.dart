import 'package:material_ui/material_ui.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/app.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Mockingbird.initialize();
  runApp(const MockingbirdDemoApp());
}
