import 'package:flutter/material.dart';
import 'package:mockingbird_example/src/demo/demo_page.dart';

/// The Auris demo: start the microphone, pick a preset, watch detections.
class AurisDemoApp extends StatelessWidget {
  const AurisDemoApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Auris Demo',
    theme: ThemeData(
      colorSchemeSeed: Colors.indigo,
      useMaterial3: true,
      brightness: Brightness.dark,
    ),
    home: const DemoPage(),
  );
}
