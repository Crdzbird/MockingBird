import 'package:mockingbird/src/events/mockingbird_event.dart';
import 'package:mockingbird/src/mockingbird_exception.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/mockingbird_stats.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Drains the engine's queues through the platform and decodes each record.
final class EnginePoller {
  /// Creates a poller for one engine handle on the registered platform.
  const factory EnginePoller(
    MockingbirdPlatform platform,
    MockingbirdEngineHandle handle, {
    required void Function(MockingbirdEvent) onEvent,
    required void Function(MockingbirdPacket) onPacket,
    required void Function(MockingbirdStats) onStats,
  }) = EnginePoller._;

  const EnginePoller._(
    this._platform,
    this._handle, {
    required this.onEvent,
    required this.onPacket,
    required this.onStats,
  });

  /// Receives each decoded event.
  final void Function(MockingbirdEvent) onEvent;

  /// Receives each packet, whose bytes the receiver owns.
  final void Function(MockingbirdPacket) onPacket;

  /// Receives one stats snapshot per poll.
  final void Function(MockingbirdStats) onStats;

  final MockingbirdPlatform _platform;
  final MockingbirdEngineHandle _handle;

  /// Empties both queues, then reads the stats.
  void poll() {
    _drainEvents();
    _drainPackets();
    _readStats();
  }

  void _drainEvents() {
    while (true) {
      final r = _platform.pollEvent(_handle);
      if (r.status == MockingbirdStatusCode.queueEmpty) return;
      MockingbirdException.check(r.status, 'mockingbird_poll_event');
      onEvent(MockingbirdEvent.fromNative(r.event!));
    }
  }

  void _drainPackets() {
    while (true) {
      final r = _platform.drainPacket(_handle);
      if (r.status == MockingbirdStatusCode.queueEmpty) return;
      MockingbirdException.check(r.status, 'mockingbird_drain');
      onPacket(MockingbirdPacket.fromNative(r.packet!));
    }
  }

  void _readStats() {
    final r = _platform.stats(_handle);
    MockingbirdException.check(r.status, 'mockingbird_get_stats');
    onStats(MockingbirdStats.fromNative(r.stats!));
  }
}
