import 'dart:async';
import 'dart:typed_data';

import 'package:mockingbird/src/lifecycle_queue.dart';
import 'package:mockingbird/src/mockingbird_exception.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/player/mockingbird_player_config.dart';
import 'package:mockingbird/src/player/mockingbird_player_stats.dart';
import 'package:mockingbird/src/player/player_focus.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

part 'mockingbird_player_controls.dart';
part 'mockingbird_player_feed.dart';
part 'mockingbird_player_lifecycle.dart';

/// Plays Opus or PCM packets on the device's output through a jitter
/// buffer. Create it, [MockingbirdPlayerLifecycle.start] it, push packets
/// as they arrive, listen to [stats], then [dispose] it.
final class MockingbirdPlayer {
  MockingbirdPlayer._(this._platform, this._handle, this.config);

  /// Creates a player for [config]; throws [MockingbirdException] if
  /// rejected.
  factory MockingbirdPlayer.create([
    MockingbirdPlayerConfig config = const MockingbirdPlayerConfig(),
  ]) {
    final platform = MockingbirdPlatform.instance;
    final r = platform.createPlayer(config.toNative());
    MockingbirdException.check(r.status, 'mockingbird_player_create');
    return MockingbirdPlayer._(platform, r.handle!, config);
  }

  /// The configuration the player was created with.
  final MockingbirdPlayerConfig config;

  final MockingbirdPlatform _platform;
  final MockingbirdPlayerHandle _handle;
  final _stats = StreamController<MockingbirdPlayerStats>.broadcast();
  late final PlayerFocus _focus = PlayerFocus(
    onPause: _pauseForFocus,
    onResume: _resumeAfterFocus,
    onDuck: _duck,
  );
  Timer? _timer;
  final _queue = LifecycleQueue();
  double _volume = 1;
  bool _muted = false;
  bool _disposed = false;
  bool _focusPaused = false;

  /// A counters snapshot per stats tick while the player runs.
  Stream<MockingbirdPlayerStats> get stats => _stats.stream;

  /// True from the moment start completes until stop runs.
  bool get isRunning => _timer != null;

  /// Reads the counters now.
  MockingbirdPlayerStats readStats() {
    _ensureLive();
    final r = _platform.playerStats(_handle);
    MockingbirdException.check(r.status, 'mockingbird_player_get_stats');
    return MockingbirdPlayerStats.fromNative(r.stats!);
  }

  /// Stops if needed, after any start, pause, resume or stop still pending,
  /// and destroys the native player. Idempotent.
  Future<void> dispose() async {
    if (_disposed) return;
    _disposed = true;
    try {
      await _queue.add(_halt);
    } finally {
      await _platform.destroyPlayer(_handle);
      await _stats.close();
    }
  }

  void _tick() {
    if (_disposed || _stats.isClosed) return;
    final r = _platform.playerStats(_handle);
    if (r.status == MockingbirdStatusCode.ok) {
      _stats.add(MockingbirdPlayerStats.fromNative(r.stats!));
    }
  }

  void _ensureLive() {
    if (_disposed) throw StateError('MockingbirdPlayer has been disposed.');
  }
}
