import 'package:flutter/material.dart';
import 'package:mockingbird_example/src/demo/demo_page.dart';

/// The Mockingbird demo: start the microphone, pick a preset, watch detections.
final class MockingbirdDemoApp extends StatelessWidget {
  const factory MockingbirdDemoApp({Key? key}) = MockingbirdDemoApp._;

  const MockingbirdDemoApp._({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Mockingbird Demo',
    theme: ThemeData(
      colorSchemeSeed: Colors.indigo,
      useMaterial3: true,
      brightness: Brightness.dark,
    ),
    home: const DemoPage(),
  );
}
