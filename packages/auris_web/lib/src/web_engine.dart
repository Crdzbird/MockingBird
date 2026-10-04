import 'dart:js_interop';
import 'dart:typed_data';

import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:auris_web/src/auris_module.dart';
import 'package:auris_web/src/web_config_writer.dart';
import 'package:auris_web/src/web_decoders.dart';
import 'package:auris_web/src/web_engine_handle.dart';
import 'package:auris_web/src/web_pcm.dart';

/// Engine operations over the WebAssembly module.
final class WebEngine {
  const WebEngine(this._m);

  static const _hostPcm = 4;

  final AurisModule _m;

  AurisCreateResult create(AurisNativeConfig config) {
    final cfg = WebConfigWriter(_m).write(config);
    final statusPtr = _m.malloc(4);
    try {
      final engine = _m.auris_web_create(cfg, statusPtr);
      final status = _m.HEAP32.toDart[statusPtr >> 2];
      if (engine == 0) return (status: status, handle: null);
      final handle = WebEngineHandle(
        _m,
        engine,
        captures: config.backend != _hostPcm,
        sampleRateHz: config.sampleRateHz,
      );
      return (status: status, handle: handle);
    } finally {
      _m
        ..free(statusPtr)
        ..auris_web_config_free(cfg);
    }
  }

  void destroy(WebEngineHandle h) {
    _m.auris_destroy(h.pointer);
    h.free(_m);
  }

  int pushPcm(WebEngineHandle h, Int16List samples, int captureTsUs) =>
      WebPcm(_m).push(h, samples, captureTsUs);

  AurisPcmResult readPcm(WebEngineHandle h, int maxSamples) =>
      WebPcm(_m).read(h, maxSamples);

  AurisEventResult pollEvent(WebEngineHandle h) {
    final s = _m.auris_poll_event(h.pointer, h.event);
    if (s != AurisStatusCode.ok) return (status: s, event: null);
    return (status: s, event: WebDecoders(_m).event(h.event));
  }

  AurisPacketResult drainPacket(WebEngineHandle h) {
    final s = _m.auris_drain(h.pointer, h.packet);
    if (s != AurisStatusCode.ok) return (status: s, packet: null);
    final packet = WebDecoders(_m).packet(h.packet);
    final released = _m.auris_release(h.pointer, h.packet);
    if (released != AurisStatusCode.ok) return (status: released, packet: null);
    return (status: s, packet: packet);
  }

  AurisStatsResult stats(WebEngineHandle h) {
    final s = _m.auris_get_stats(h.pointer, h.stats);
    if (s != AurisStatusCode.ok) return (status: s, stats: null);
    return (status: s, stats: WebDecoders(_m).stats(h.stats));
  }

  int updateConfig(WebEngineHandle h, AurisNativeConfig config) {
    final cfg = WebConfigWriter(_m).write(config);
    try {
      return _m.auris_update_config(h.pointer, cfg);
    } finally {
      _m.auris_web_config_free(cfg);
    }
  }
}
