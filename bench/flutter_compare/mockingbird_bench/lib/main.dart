import 'dart:io';

import 'package:mockingbird_bench/mockingbird_run.dart';
import 'package:mockingbird_bench/mockingbird_scenarios.dart';
import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('mockingbird', paths);
  try {
    for (final scenario in MockingbirdScenarios(paths).all) {
      await runner.run(
        scenario.name,
        fixture.seconds,
        () => MockingbirdRun(scenario.config, fixture).execute(),
      );
    }
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
