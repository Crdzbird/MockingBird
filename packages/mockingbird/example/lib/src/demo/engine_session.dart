import 'dart:async';

import 'package:mockingbird/mockingbird.dart';

/// One running engine together with the subscriptions that feed the UI,
/// so they always open and close as a unit.
final class EngineSession {
  EngineSession._(this.engine, this._subscriptions);

  /// Creates and starts an engine; the callbacks receive its streams.
  static Future<EngineSession> open(
    AurisConfig config, {
    required void Function(AurisEvent) onEvent,
    required void Function(AurisStats) onStats,
    required void Function(AurisPacket) onPacket,
  }) async {
    final engine = AurisEngine.create(config);
    final subscriptions = <StreamSubscription<Object>>[
      engine.events.listen(onEvent),
      engine.stats.listen(onStats),
      engine.packets.listen(onPacket),
    ];
    try {
      engine.start();
    } on AurisException {
      await engine.dispose();
      rethrow;
    }
    return EngineSession._(engine, subscriptions);
  }

  final AurisEngine engine;
  final List<StreamSubscription<Object>> _subscriptions;

  /// Cancels the subscriptions, then stops and destroys the engine.
  Future<void> close() async {
    for (final s in _subscriptions) {
      await s.cancel();
    }
    engine.stop();
    await engine.dispose();
  }
}
