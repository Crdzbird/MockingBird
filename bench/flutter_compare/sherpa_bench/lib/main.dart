import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:harness/harness.dart';
import 'package:sherpa_bench/sherpa_denoise_run.dart';
import 'package:sherpa_bench/sherpa_tagging_run.dart';
import 'package:sherpa_bench/sherpa_vad_run.dart';
import 'package:sherpa_onnx/sherpa_onnx.dart' as sherpa;

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  sherpa.initBindings();
  final paths = BenchPaths.fromEnvironment();
  final fixture = Fixture.standard(paths);
  final runner = BenchRunner('sherpa_onnx', paths);
  const ced = 'sherpa-onnx-ced-tiny-audio-tagging-2024-04-19';
  try {
    await runner.run(
      'vad_neural',
      fixture.seconds,
      () => SherpaVadRun(fixture, paths.model('silero_vad.onnx')).execute(),
    );
    await runner.run(
      'denoise',
      fixture.seconds,
      () => SherpaDenoiseRun(fixture, paths.model('gtcrn_simple.onnx')).execute(),
    );
    for (final file in ['model.onnx', 'model.int8.onnx']) {
      await runner.run(
        'sound_class_${file == 'model.onnx' ? 'fp32' : 'int8'}',
        fixture.seconds,
        () => SherpaTaggingRun(
          fixture,
          paths.model('$ced/$file'),
          paths.model('$ced/class_labels_indices.csv'),
        ).execute(),
      );
    }
  } on Object catch (error, stack) {
    stderr.writeln('$error\n$stack');
    exit(1);
  }
  await runner.close();
  exit(0);
}
