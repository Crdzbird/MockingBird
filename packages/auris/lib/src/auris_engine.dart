import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:auris/src/auris_exception.dart';
import 'package:auris/src/auris_packet.dart';
import 'package:auris/src/auris_stats.dart';
import 'package:auris/src/config/auris_config.dart';
import 'package:auris/src/events/auris_event.dart';
import 'package:auris/src/native/native_config.dart';
import 'package:auris/src/native/native_pcm.dart';
import 'package:auris/src/native/native_poller.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';

part 'auris_engine_lifecycle.dart';
part 'auris_engine_pcm.dart';
part 'auris_engine_tuning.dart';

/// One native engine. Create it, [start] it, listen to [events], [packets]
/// and [stats], then [dispose] it.
final class AurisEngine {
  AurisEngine._(this._bindings, this._engine, this._config);

  /// Creates an engine for [config]; throws [AurisException] if rejected.
  factory AurisEngine.create(AurisConfig config) {
    final bindings = AurisPlatform.instance.bindings;
    final native = NativeConfig.from(bindings, config);
    final out = calloc<ffi.Pointer<auris_engine_t>>();
    try {
      AurisException.check(
        bindings.auris_create(native.pointer, out),
        'auris_create',
      );
      return AurisEngine._(bindings, out.value, config);
    } finally {
      native.free();
      calloc.free(out);
    }
  }

  final AurisBindings _bindings;
  final ffi.Pointer<auris_engine_t> _engine;
  final _events = StreamController<AurisEvent>.broadcast();
  final _packets = StreamController<AurisPacket>.broadcast();
  final _stats = StreamController<AurisStats>.broadcast();
  late final _poller = NativePoller(
    _bindings,
    _engine,
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
    _poller.dispose();
    _bindings.auris_destroy(_engine);
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
