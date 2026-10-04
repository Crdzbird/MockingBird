import 'dart:ffi';

import 'package:auris_platform_interface/src/auris_platform.dart';
import 'package:auris_platform_interface/src/auris_status_code.dart';
import 'package:auris_platform_interface/src/bindings.dart';
import 'package:auris_platform_interface/src/ffi/ffi_decoders.dart';
import 'package:auris_platform_interface/src/ffi/ffi_engine_handle.dart';

/// The host-facing queues over the FFI bindings.
final class FfiQueues {
  /// Wraps [_bindings].
  const FfiQueues(this._bindings);

  final AurisBindings _bindings;

  /// Polls one event.
  AurisEventResult pollEvent(FfiEngineHandle h) {
    final s = _bindings.auris_poll_event(h.pointer, h.event).value;
    if (s != AurisStatusCode.ok) return (status: s, event: null);
    return (status: s, event: FfiDecoders.event(h.event.ref));
  }

  /// Drains one packet and releases its slot.
  AurisPacketResult drainPacket(FfiEngineHandle h) {
    final s = _bindings.auris_drain(h.pointer, h.packet).value;
    if (s != AurisStatusCode.ok) return (status: s, packet: null);
    final packet = FfiDecoders.packet(h.packet.ref);
    final released = _bindings.auris_release(h.pointer, h.packet).value;
    if (released != AurisStatusCode.ok) return (status: released, packet: null);
    return (status: s, packet: packet);
  }

  /// Reads the stats.
  AurisStatsResult stats(FfiEngineHandle h) {
    final s = _bindings.auris_get_stats(h.pointer, h.stats).value;
    if (s != AurisStatusCode.ok) return (status: s, stats: null);
    return (status: s, stats: FfiDecoders.stats(h.stats.ref));
  }
}
