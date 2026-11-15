import 'dart:async';
import 'dart:typed_data';

import 'package:mockingbird/src/config/mockingbird_config.dart';
import 'package:mockingbird/src/events/mockingbird_event.dart';
import 'package:mockingbird/src/lifecycle_queue.dart';
import 'package:mockingbird/src/mockingbird_exception.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/mockingbird_segment.dart';
import 'package:mockingbird/src/mockingbird_stats.dart';
import 'package:mockingbird/src/native/engine_poller.dart';
import 'package:mockingbird/src/native/engine_streams.dart';
import 'package:mockingbird/src/native/native_config_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

part 'mockingbird_engine_devices.dart';
part 'mockingbird_engine_keywords.dart';
part 'mockingbird_engine_lifecycle.dart';
part 'mockingbird_engine_pcm.dart';
part 'mockingbird_engine_speaker.dart';
part 'mockingbird_engine_tuning.dart';

/// One native engine. Create it, [start] it, listen to [events], [packets],
/// [segments] and [stats], then [dispose] it.
final class MockingbirdEngine {
  MockingbirdEngine._(this._platform, this._handle, this._config);

  /// Creates an engine for [config]; throws [MockingbirdException] if rejected.
  factory MockingbirdEngine.create(MockingbirdConfig config) {
    final platform = MockingbirdPlatform.instance;
    final r = platform.create(const NativeConfigMapper().map(config));
    MockingbirdException.check(r.status, 'mockingbird_create');
    return MockingbirdEngine._(platform, r.handle!, config);
  }

  final MockingbirdPlatform _platform;
  final MockingbirdEngineHandle _handle;
  late final _out = EngineStreams(sampleRateHz: _config.sampleRateHz);
  late final _poller = EnginePoller(
    _platform,
    _handle,
    onEvent: _out.onEvent,
    onPacket: _out.onPacket,
    onStats: _out.stats.add,
  );
  MockingbirdConfig _config;
  Timer? _timer;
  final _queue = LifecycleQueue();
  bool _disposed = false;

  /// Detections and lifecycle notices, in order.
  Stream<MockingbirdEvent> get events => _out.events.stream;

  /// Encoded audio, segment by segment.
  Stream<MockingbirdPacket> get packets => _out.packets.stream;

  /// Each segment once it has ended, with all its packets; `toFile` turns
  /// one into a WAV or Ogg Opus file. Only listened-to segments are kept.
  Stream<MockingbirdSegment> get segments => _out.segments;

  /// A counters snapshot per timer tick.
  Stream<MockingbirdStats> get stats => _out.stats.stream;

  /// The configuration in effect, including live retunes.
  MockingbirdConfig get config => _config;

  /// True between [start] and [stop].
  bool get isRunning => _timer != null;

  /// Stops if needed, off this isolate and after any pending [startAsync],
  /// and destroys the native engine. Idempotent.
  Future<void> dispose() async {
    if (_disposed) return;
    _disposed = true;
    try {
      await _queue.add(_stopAsync);
    } finally {
      _platform.destroy(_handle);
      await _out.close();
    }
  }

  void _ensureLive() {
    if (_disposed) throw StateError('MockingbirdEngine has been disposed.');
  }
}
