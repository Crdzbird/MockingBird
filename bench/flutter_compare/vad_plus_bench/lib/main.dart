import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';
import 'package:vad_plus_bench/vad_plus_run.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('vad_plus', paths);
  try {
    await runner.run(
      'vad_neural',
      fixture.seconds,
      () => VadPlusRun(fixture).execute(),
    );
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
