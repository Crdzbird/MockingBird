import 'dart:ffi';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_decoders.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The host-facing queues over the FFI bindings.
final class FfiQueues {
  /// Wraps [_bindings].
  const FfiQueues(this._bindings);

  final MockingbirdBindings _bindings;

  /// Polls one event.
  MockingbirdEventResult pollEvent(FfiEngineHandle h) {
    final s = _bindings.mockingbird_poll_event(h.pointer, h.event).value;
    if (s != MockingbirdStatusCode.ok) return (status: s, event: null);
    return (status: s, event: FfiDecoders.event(h.event.ref));
  }

  /// Drains one packet and releases its slot.
  MockingbirdPacketResult drainPacket(FfiEngineHandle h) {
    final s = _bindings.mockingbird_drain(h.pointer, h.packet).value;
    if (s != MockingbirdStatusCode.ok) return (status: s, packet: null);
    final packet = FfiDecoders.packet(h.packet.ref);
    final released = _bindings.mockingbird_release(h.pointer, h.packet).value;
    if (released != MockingbirdStatusCode.ok) {
      return (status: released, packet: null);
    }
    return (status: s, packet: packet);
  }

  /// Reads the stats.
  MockingbirdStatsResult stats(FfiEngineHandle h) {
    final s = _bindings.mockingbird_get_stats(h.pointer, h.stats).value;
    if (s != MockingbirdStatusCode.ok) return (status: s, stats: null);
    return (status: s, stats: FfiDecoders.stats(h.stats.ref));
  }
}
