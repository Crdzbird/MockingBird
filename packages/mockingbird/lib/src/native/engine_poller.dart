import 'package:mockingbird/src/auris_exception.dart';
import 'package:mockingbird/src/auris_packet.dart';
import 'package:mockingbird/src/auris_stats.dart';
import 'package:mockingbird/src/events/auris_event.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Drains the engine's queues through the platform and decodes each record.
final class EnginePoller {
  /// Creates a poller for [handle] on [platform].
  const EnginePoller(
    this._platform,
    this._handle, {
    required this.onEvent,
    required this.onPacket,
    required this.onStats,
  });

  /// Receives each decoded event.
  final void Function(AurisEvent) onEvent;

  /// Receives each packet, whose bytes the receiver owns.
  final void Function(AurisPacket) onPacket;

  /// Receives one stats snapshot per poll.
  final void Function(AurisStats) onStats;

  final AurisPlatform _platform;
  final AurisEngineHandle _handle;

  /// Empties both queues, then reads the stats.
  void poll() {
    _drainEvents();
    _drainPackets();
    _readStats();
  }

  void _drainEvents() {
    while (true) {
      final r = _platform.pollEvent(_handle);
      if (r.status == AurisStatusCode.queueEmpty) return;
      AurisException.check(r.status, 'auris_poll_event');
      onEvent(AurisEvent.fromNative(r.event!));
    }
  }

  void _drainPackets() {
    while (true) {
      final r = _platform.drainPacket(_handle);
      if (r.status == AurisStatusCode.queueEmpty) return;
      AurisException.check(r.status, 'auris_drain');
      onPacket(AurisPacket.fromNative(r.packet!));
    }
  }

  void _readStats() {
    final r = _platform.stats(_handle);
    AurisException.check(r.status, 'auris_get_stats');
    onStats(AurisStats.fromNative(r.stats!));
  }
}
