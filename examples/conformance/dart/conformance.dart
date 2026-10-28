// Dart peer of the Mockingbird cross-binding conformance harness, the mirror
// of examples/conformance/go/main.go. It drives the same native
// libmockingbird through the Dart FFI bindings the Flutter plugin uses,
// exchanges deterministic audio with another peer over TCP, and checks that
// both bindings turn identical PCM into byte-identical packets, events and
// stats. The signal, configuration, processing order and FNV-1a/64 digest
// are the same in every peer.
//
// Start the listening peer first, then:
//   dart run conformance.dart 127.0.0.1:9999 [libmockingbird-path] [--peer=go]
//   dart run conformance.dart dump [libmockingbird-path]

import 'dart:ffi';
import 'dart:io';

import 'package:mockingbird_conformance/src/engine_run.dart';
import 'package:mockingbird_conformance/src/library_path.dart';
import 'package:mockingbird_conformance/src/session.dart';
import 'package:mockingbird_conformance/src/signal.dart';
import 'package:mockingbird_platform_interface/ffi.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    stderr.writeln(
      'usage: dart run conformance.dart host:port [libmockingbird-path]',
    );
    exit(2);
  }
  final dump = args[0] == 'dump';
  final libPath = resolveLib(args);
  final run = EngineRun(
    MockingbirdBindings(DynamicLibrary.open(libPath)),
    dump: dump,
  );
  if (dump) {
    run.process(genSignal(goSeed));
    return;
  }
  final hp = args[0].split(':');
  final peerName = args
      .firstWhere((a) => a.startsWith('--peer='), orElse: () => '--peer=go')
      .substring('--peer='.length);
  final pass = await runSession(run, hp[0], int.parse(hp[1]), peerName);
  if (!pass) {
    stdout.writeln('\nCONFORMANCE FAIL: divergence detected');
    exit(1);
  }
  stdout.writeln(
    '\nCONFORMANCE PASS: Dart and $peerName produce byte-identical results '
    'both ways',
  );
}
