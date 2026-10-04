import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';
import 'package:nnnoiseless_bench/nnnoiseless_run.dart';
import 'package:nnnoiseless_bench/rust_library.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('flutter_nnnoiseless', paths);
  try {
    await const RustLibrary().load();
    await runner.run(
      'denoise',
      fixture.seconds,
      () => NnnoiselessRun(fixture).execute(),
    );
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
