import 'dart:ffi' as ffi;

import 'package:mockingbird_platform_interface/src/auris_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:ffi/ffi.dart';

/// A native engine pointer plus the scratch records polling reuses.
final class FfiEngineHandle extends AurisEngineHandle {
  /// Wraps [pointer] and allocates the scratch records once.
  FfiEngineHandle(this.pointer)
      : event = calloc(),
        packet = calloc(),
        stats = calloc();

  /// The engine.
  final ffi.Pointer<auris_engine_t> pointer;
  /// Scratch event record.
  final ffi.Pointer<auris_event_t> event;
  /// Scratch packet record.
  final ffi.Pointer<auris_packet_t> packet;
  /// Scratch stats record.
  final ffi.Pointer<auris_stats_t> stats;

  /// Frees the scratch records; the engine itself is destroyed separately.
  void free() {
    calloc
      ..free(event)
      ..free(packet)
      ..free(stats);
  }
}
