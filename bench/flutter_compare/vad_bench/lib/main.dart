import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';
import 'package:vad_bench/vad_run.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('vad', paths);
  try {
    await runner.run(
      'vad_neural',
      fixture.seconds,
      () => VadRun(fixture).execute(),
    );
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
