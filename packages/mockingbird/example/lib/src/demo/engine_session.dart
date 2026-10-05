import 'dart:async';

import 'package:mockingbird/mockingbird.dart';

/// One running engine together with the subscriptions that feed the UI,
/// so they always open and close as a unit.
final class EngineSession {
  EngineSession._(this.engine, this._subscriptions);

  /// Creates and starts an engine; the callbacks receive its streams.
  static Future<EngineSession> open(
    MockingbirdConfig config, {
    required void Function(MockingbirdEvent) onEvent,
    required void Function(MockingbirdStats) onStats,
    required void Function(MockingbirdPacket) onPacket,
  }) async {
    final engine = MockingbirdEngine.create(config);
    final subscriptions = <StreamSubscription<Object>>[
      engine.events.listen(onEvent),
      engine.stats.listen(onStats),
      engine.packets.listen(onPacket),
    ];
    try {
      engine.start();
    } on MockingbirdException {
      await engine.dispose();
      rethrow;
    }
    return EngineSession._(engine, subscriptions);
  }

  final MockingbirdEngine engine;
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
