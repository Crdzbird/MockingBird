import 'package:auris/auris.dart';
import 'package:auris_example/src/app.dart';
import 'package:flutter/material.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Auris.initialize();
  runApp(const AurisDemoApp());
}
