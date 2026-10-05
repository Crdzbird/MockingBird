import 'dart:js_interop';

/// The Emscripten module: exported C functions and the heap views.
///
/// Heap views are read on every access because the memory can grow.
extension type AurisModule(JSObject _) implements JSObject {
  external JSUint8Array get HEAPU8;
  external JSInt16Array get HEAP16;
  external JSInt32Array get HEAP32;
  @JS('UTF8ToString')
  external JSString utf8ToString(int pointer);
  @JS('_malloc')
  external int malloc(int bytes);
  @JS('_free')
  external void free(int pointer);

  @JS('_auris_web_config_new')
  external int auris_web_config_new();
  @JS('_auris_web_config_free')
  external void auris_web_config_free(int cfg);
  @JS('_auris_web_config_set')
  external int auris_web_config_set(int cfg, int field, double value);
  @JS('_auris_web_create')
  external int auris_web_create(int cfg, int outStatus);
  @JS('_auris_destroy')
  external void auris_destroy(int engine);
  @JS('_auris_start')
  external int auris_start(int engine);
  @JS('_auris_stop')
  external int auris_stop(int engine);
  @JS('_auris_web_push_pcm')
  external int auris_web_push_pcm(int engine, int samples, int count, double ts);
  @JS('_auris_read_pcm')
  external int auris_read_pcm(int engine, int out, int max, int outWritten);
  @JS('_auris_poll_event')
  external int auris_poll_event(int engine, int event);
  @JS('_auris_drain')
  external int auris_drain(int engine, int packet);
  @JS('_auris_release')
  external int auris_release(int engine, int packet);
  @JS('_auris_get_stats')
  external int auris_get_stats(int engine, int stats);
  @JS('_auris_update_config')
  external int auris_update_config(int engine, int cfg);
  @JS('_auris_version_string')
  external int auris_version_string();
  @JS('_auris_status_str')
  external int auris_status_str(int status);

  @JS('_auris_web_event_new')
  external int auris_web_event_new();
  @JS('_auris_web_event_free')
  external void auris_web_event_free(int event);
  @JS('_auris_web_event_get')
  external double auris_web_event_get(int event, int field);
  @JS('_auris_web_packet_new')
  external int auris_web_packet_new();
  @JS('_auris_web_packet_free')
  external void auris_web_packet_free(int packet);
  @JS('_auris_web_packet_get')
  external double auris_web_packet_get(int packet, int field);
  @JS('_auris_web_stats_new')
  external int auris_web_stats_new();
  @JS('_auris_web_stats_free')
  external void auris_web_stats_free(int stats);
  @JS('_auris_web_stats_get')
  external double auris_web_stats_get(int stats, int field);
}

/// The default export of `auris.js`: a factory for [AurisModule].
extension type AurisModuleFactory(JSFunction _) implements JSFunction {
  external JSPromise<AurisModule> call(JSObject options);
}
