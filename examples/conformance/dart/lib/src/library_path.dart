import 'dart:io';

/// The library named on the command line, or the repository's build tree.
String resolveLib(List<String> args) {
  final positional = args.where((a) => !a.startsWith('--')).toList();
  if (positional.length >= 2) return positional[1];
  final ext = Platform.isMacOS ? 'dylib' : (Platform.isWindows ? 'dll' : 'so');
  final base = Platform.script.resolve('../../../build/');
  for (final name in ['libmockingbird.$ext', 'libmockingbird.0.1.0.$ext']) {
    final p = base.resolve(name).toFilePath();
    if (File(p).existsSync()) return p;
  }
  return base.resolve('libmockingbird.$ext').toFilePath();
}
