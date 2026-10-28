import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_conformance/src/output_digest.dart';
import 'package:mockingbird_conformance/src/proc_result.dart';
import 'package:mockingbird_conformance/src/signal.dart';
import 'package:mockingbird_platform_interface/ffi.dart';

/// Runs one signal through a fresh engine and hashes what it emits:
/// packets, then events, then four stats counters, the order every peer uses.
final class EngineRun {
  /// Runs over [bindings]; [dump] also prints every record.
  const factory EngineRun(MockingbirdBindings bindings, {bool dump}) =
      EngineRun._;

  const EngineRun._(this._b, {this.dump = false});

  final MockingbirdBindings _b;

  /// Whether to print every record.
  final bool dump;

  /// Pushes [pcm] in 20 ms frames, stops, and hashes the output.
  ProcResult process(Int16List pcm) {
    final cfg = calloc<mockingbird_config_t>();
    final out = calloc<Pointer<mockingbird_engine_t>>();
    try {
      _check(
        _b.mockingbird_config_defaults(cfg),
        'mockingbird_config_defaults',
      );
      cfg.ref
        ..backendAsInt = 4
        ..codecAsInt = 1
        ..pcm_ring_frames = 1 << 17
        ..packet_queue_capacity = 4096;
      _check(_b.mockingbird_create(cfg, out), 'mockingbird_create');
      final engine = out.value;
      try {
        _check(_b.mockingbird_start(engine), 'mockingbird_start');
        _push(engine, pcm);
        _b.mockingbird_stop(engine);
        return digestOutput(_b, engine, dump: dump);
      } finally {
        _b.mockingbird_destroy(engine);
      }
    } finally {
      calloc
        ..free(cfg)
        ..free(out);
    }
  }

  void _push(Pointer<mockingbird_engine_t> engine, Int16List pcm) {
    final native = calloc<Int16>(pcm.length);
    try {
      native.asTypedList(pcm.length).setAll(0, pcm);
      for (var f = 0; f < pcm.length ~/ frameLen; f++) {
        final at = Pointer<Int16>.fromAddress(
          native.address + f * frameLen * 2,
        );
        _check(
          _b.mockingbird_push_pcm(engine, at, frameLen, f * 20000),
          'mockingbird_push_pcm',
        );
      }
    } finally {
      calloc.free(native);
    }
  }

  static void _check(mockingbird_status s, String what) {
    if (s != .MOCKINGBIRD_OK) throw StateError('$what failed');
  }
}
