import 'dart:async';
import 'dart:typed_data';

import 'package:mockingbird/src/auris_exception.dart';
import 'package:mockingbird/src/auris_packet.dart';
import 'package:mockingbird/src/auris_stats.dart';
import 'package:mockingbird/src/config/auris_config.dart';
import 'package:mockingbird/src/events/auris_event.dart';
import 'package:mockingbird/src/native/engine_poller.dart';
import 'package:mockingbird/src/native/native_config_mapper.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

part 'auris_engine_lifecycle.dart';
part 'auris_engine_pcm.dart';
part 'auris_engine_tuning.dart';

/// One native engine. Create it, [start] it, listen to [events], [packets]
/// and [stats], then [dispose] it.
final class AurisEngine {
  AurisEngine._(this._platform, this._handle, this._config);

  /// Creates an engine for [config]; throws [AurisException] if rejected.
  factory AurisEngine.create(AurisConfig config) {
    final platform = AurisPlatform.instance;
    final r = platform.create(const NativeConfigMapper().map(config));
    AurisException.check(r.status, 'auris_create');
    return AurisEngine._(platform, r.handle!, config);
  }

  final AurisPlatform _platform;
  final AurisEngineHandle _handle;
  final _events = StreamController<AurisEvent>.broadcast();
  final _packets = StreamController<AurisPacket>.broadcast();
  final _stats = StreamController<AurisStats>.broadcast();
  late final _poller = EnginePoller(
    _platform,
    _handle,
    onEvent: _events.add,
    onPacket: _packets.add,
    onStats: _stats.add,
  );
  AurisConfig _config;
  Timer? _timer;
  bool _disposed = false;

  /// Detections and lifecycle notices, in order.
  Stream<AurisEvent> get events => _events.stream;

  /// Encoded audio, segment by segment.
  Stream<AurisPacket> get packets => _packets.stream;

  /// A counters snapshot per poll.
  Stream<AurisStats> get stats => _stats.stream;

  /// The configuration in effect, including live retunes.
  AurisConfig get config => _config;

  /// True between [start] and [stop].
  bool get isRunning => _timer != null;

  /// Stops if needed and destroys the native engine. Idempotent.
  Future<void> dispose() async {
    if (_disposed) return;
    if (isRunning) stop();
    _disposed = true;
    _platform.destroy(_handle);
    await _events.close();
    await _packets.close();
    await _stats.close();
  }

  void _poll() {
    try {
      _poller.poll();
    } on AurisException catch (e, st) {
      _events.addError(e, st);
    }
  }

  void _ensureLive() {
    if (_disposed) throw StateError('AurisEngine has been disposed.');
  }
}
