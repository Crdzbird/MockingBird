import 'dart:io';

class BenchPaths {
  BenchPaths._(this.root);

  factory BenchPaths.fromEnvironment() {
    const root = String.fromEnvironment('AURIS_ROOT');
    if (root.isEmpty) {
      throw StateError('pass --dart-define=AURIS_ROOT=<repository root>');
    }
    return BenchPaths._(root);
  }

  final String root;

  String get fixtures => '$root/tests/fixtures';

  String get models => '$root/bench/flutter_compare/models';

  String fixture(String name) => '$fixtures/$name';

  String model(String name) => '$models/$name';

  String get resultsDir => '$root/bench/flutter_compare/results';

  File resultFile(String library) => File('$resultsDir/$library.jsonl');
}
