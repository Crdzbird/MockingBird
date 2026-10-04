import 'dart:io';

import 'package:auris_bench/auris_run.dart';
import 'package:auris_bench/auris_scenarios.dart';
import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('auris', paths);
  try {
    for (final scenario in AurisScenarios(paths).all) {
      await runner.run(
        scenario.name,
        fixture.seconds,
        () => AurisRun(scenario.config, fixture).execute(),
      );
    }
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
