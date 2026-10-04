import 'dart:ffi' as ffi;

import 'package:auris/src/auris_exception.dart';
import 'package:auris/src/auris_packet.dart';
import 'package:auris/src/auris_stats.dart';
import 'package:auris/src/events/auris_event.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';

/// Drains the engine's owned queues into Dart objects. The engine copies
/// each event into our buffer and lends each packet until it is released,
/// so everything handed to the sinks is Dart-owned.
final class NativePoller {
  /// Binds to one engine handle and three sinks.
  NativePoller(
    this._bindings,
    this._engine, {
    required this.onEvent,
    required this.onPacket,
    required this.onStats,
  });

  /// Receives each decoded event.
  final void Function(AurisEvent) onEvent;

  /// Receives each copied packet.
  final void Function(AurisPacket) onPacket;

  /// Receives one stats snapshot per poll.
  final void Function(AurisStats) onStats;

  final AurisBindings _bindings;
  final ffi.Pointer<auris_engine_t> _engine;
  final ffi.Pointer<auris_event_t> _event = calloc();
  final ffi.Pointer<auris_packet_t> _packet = calloc();
  final ffi.Pointer<auris_stats_t> _stats = calloc();

  /// Drains every queued event and packet, then reads the stats.
  void poll() {
    _drainEvents();
    _drainPackets();
    _readStats();
  }

  /// Releases the scratch buffers.
  void dispose() {
    calloc
      ..free(_event)
      ..free(_packet)
      ..free(_stats);
  }

  void _drainEvents() {
    while (true) {
      final s = _bindings.auris_poll_event(_engine, _event);
      if (s == auris_status.AURIS_ERR_QUEUE_EMPTY) return;
      AurisException.check(s, 'auris_poll_event');
      onEvent(AurisEvent.fromNative(_event.ref));
    }
  }

  void _drainPackets() {
    while (true) {
      final s = _bindings.auris_drain(_engine, _packet);
      if (s == auris_status.AURIS_ERR_QUEUE_EMPTY) return;
      AurisException.check(s, 'auris_drain');
      final packet = AurisPacket.fromNative(_packet.ref);
      AurisException.check(
        _bindings.auris_release(_engine, _packet),
        'auris_release',
      );
      onPacket(packet);
    }
  }

  void _readStats() {
    AurisException.check(
      _bindings.auris_get_stats(_engine, _stats),
      'auris_get_stats',
    );
    onStats(AurisStats.fromNative(_stats.ref));
  }
}
