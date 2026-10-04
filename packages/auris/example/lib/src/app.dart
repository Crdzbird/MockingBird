import 'package:auris_example/src/demo/demo_page.dart';
import 'package:flutter/material.dart';

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
