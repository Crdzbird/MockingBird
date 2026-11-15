import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_player_handle.dart';

/// Player creation, lifecycle, controls and stats over the FFI bindings.
final class FfiPlayers {
  /// Wraps [_bindings].
  const factory FfiPlayers(MockingbirdBindings bindings) = FfiPlayers._;

  const FfiPlayers._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Creates a player from [c].
  MockingbirdPlayerCreateResult create(MockingbirdNativePlayerConfig c) {
    final cfg = calloc<mockingbird_player_config_t>();
    final out = calloc<ffi.Pointer<mockingbird_player_t>>();
    try {
      _bindings.mockingbird_player_config_defaults(cfg);
      cfg.ref
        ..backendAsInt = c.backend
        ..sample_rate_hz = c.sampleRateHz
        ..channels = c.channels
        ..target_delay_ms = c.targetDelayMs
        ..max_delay_ms = c.maxDelayMs
        ..conceal_max_ms = c.concealMaxMs
        ..queue_packets = c.queuePackets;
      final s = _bindings.mockingbird_player_create(cfg, out).value;
      if (s != MockingbirdStatusCode.ok) return (status: s, handle: null);
      return (
        status: s,
        handle: FfiPlayerHandle(out.value, channels: c.channels),
      );
    } finally {
      calloc
        ..free(cfg)
        ..free(out);
    }
  }

  /// Destroys the player, which first runs what its thread has queued,
  /// then frees its callback and scratch memory.
  void destroy(FfiPlayerHandle h) {
    _bindings.mockingbird_player_destroy(h.pointer);
    h.lifecycle.close();
    h.free();
  }

  /// Queues [op] on the player's lifecycle thread; completes with its status.
  Future<int> run(FfiPlayerHandle h, mockingbird_player_op op) =>
      h.lifecycle.run(
        (post, port) => _bindings
            .mockingbird_player_submit_port(h.pointer, op, post, port)
            .value,
      );

  /// Drops everything pushed so far.
  int flush(FfiPlayerHandle h) =>
      _bindings.mockingbird_player_flush(h.pointer).value;

  /// Sets the gain.
  int volume(FfiPlayerHandle h, double v) =>
      _bindings.mockingbird_player_set_volume(h.pointer, v).value;

  /// Mutes or unmutes.
  int muted(FfiPlayerHandle h, {required bool muted}) =>
      _bindings.mockingbird_player_set_muted(h.pointer, muted ? 1 : 0).value;

  /// Reads the stats into a plain record.
  MockingbirdPlayerStatsResult stats(FfiPlayerHandle h) {
    final s = _bindings.mockingbird_player_get_stats(h.pointer, h.stats).value;
    if (s != MockingbirdStatusCode.ok) return (status: s, stats: null);
    final r = h.stats.ref;
    return (
      status: s,
      stats: MockingbirdNativePlayerStats(
        packetsReceived: r.packets_received,
        packetsPlayed: r.packets_played,
        packetsConcealed: r.packets_concealed,
        packetsDropped: r.packets_dropped,
        packetsLate: r.packets_late,
        underruns: r.underruns,
        framesRendered: r.frames_rendered,
        bufferedPackets: r.buffered_packets,
        bufferedMs: r.buffered_ms,
        state: r.state,
        outputRestarts: r.output_restarts,
      ),
    );
  }
}
