import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_record_api.dart';
import 'package:mockingbird_web/src/web_config_writer.dart';
import 'package:mockingbird_web/src/web_decoders.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_pcm.dart';

/// Engine operations over the WebAssembly module.
final class WebEngine {
  /// Wraps the loaded module.
  const WebEngine(this._m);

  static const _hostPcm = 4;

  final MockingbirdModule _m;

  /// The linked engine version.
  String version() => _m.utf8ToString(_m.versionString()).toDart;

  /// The engine's name for [status].
  String statusName(int status) => _m.utf8ToString(_m.statusStr(status)).toDart;

  /// Creates an engine from a config record.
  MockingbirdCreateResult create(MockingbirdNativeConfig config) {
    final cfg = WebConfigWriter(_m).write(config);
    final statusPtr = _m.malloc(4);
    try {
      final engine = MockingbirdRecordApi(_m).createEngine(cfg, statusPtr);
      final status = _m.heap32.toDart[statusPtr >> 2];
      if (engine == 0) return (status: status, handle: null);
      final handle = WebEngineHandle(
        _m,
        engine,
        captures: config.backend != _hostPcm,
        sampleRateHz: config.sampleRateHz,
      );
      return (status: status, handle: handle);
    } finally {
      _m.free(statusPtr);
      MockingbirdRecordApi(_m).configFree(cfg);
    }
  }

  /// Destroys the engine and its scratch records.
  void destroy(WebEngineHandle h) {
    _m.destroy(h.pointer);
    h.free(_m);
  }

  /// Pushes samples into the engine.
  int pushPcm(WebEngineHandle h, Int16List samples, int captureTsUs) =>
      WebPcm(_m).push(h, samples, captureTsUs);

  /// Reads the PCM tap.
  MockingbirdPcmResult readPcm(WebEngineHandle h, int maxSamples) =>
      WebPcm(_m).read(h, maxSamples);

  /// Takes the next event.
  MockingbirdEventResult pollEvent(WebEngineHandle h) {
    final s = _m.pollEvent(h.pointer, h.event);
    if (s != MockingbirdStatusCode.ok) return (status: s, event: null);
    return (status: s, event: WebDecoders(_m).event(h.event));
  }

  /// Takes the next packet and releases its slot.
  MockingbirdPacketResult drainPacket(WebEngineHandle h) {
    final s = _m.drain(h.pointer, h.packet);
    if (s != MockingbirdStatusCode.ok) return (status: s, packet: null);
    final packet = WebDecoders(_m).packet(h.packet);
    final released = _m.release(h.pointer, h.packet);
    if (released != MockingbirdStatusCode.ok) {
      return (status: released, packet: null);
    }
    return (status: s, packet: packet);
  }

  /// Reads a stats snapshot.
  MockingbirdStatsResult stats(WebEngineHandle h) {
    final s = _m.getStats(h.pointer, h.stats);
    if (s != MockingbirdStatusCode.ok) return (status: s, stats: null);
    return (status: s, stats: WebDecoders(_m).stats(h.stats));
  }

  /// Retunes the running engine.
  int updateConfig(WebEngineHandle h, MockingbirdNativeConfig config) {
    final cfg = WebConfigWriter(_m).write(config);
    try {
      return _m.updateConfig(h.pointer, cfg);
    } finally {
      MockingbirdRecordApi(_m).configFree(cfg);
    }
  }
}
