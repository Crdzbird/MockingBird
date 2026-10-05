import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/app.dart';
import 'package:flutter/material.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Auris.initialize();
  runApp(const AurisDemoApp());
}
