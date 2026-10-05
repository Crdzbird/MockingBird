import 'dart:js_interop';

/// The Emscripten module: heap views and the engine's lifecycle and queue
/// entry points. Heap views are read on every access because memory grows.
extension type AurisModule(JSObject _) implements JSObject {
  /// The byte view of linear memory.
  @JS('HEAPU8')
  external JSUint8Array get heapU8;

  /// The int16 view of linear memory.
  @JS('HEAP16')
  external JSInt16Array get heap16;

  /// The int32 view of linear memory.
  @JS('HEAP32')
  external JSInt32Array get heap32;

  /// Reads a NUL-terminated string at [pointer].
  @JS('UTF8ToString')
  external JSString utf8ToString(int pointer);

  /// Allocates [bytes] in linear memory.
  @JS('_malloc')
  external int malloc(int bytes);

  /// Frees a [malloc] allocation.
  @JS('_free')
  external void free(int pointer);

  /// Destroys an engine.
  @JS('_auris_destroy')
  external void destroy(int engine);

  /// Starts an engine.
  @JS('_auris_start')
  external int start(int engine);

  /// Stops an engine; pushed frames are processed first.
  @JS('_auris_stop')
  external int stop(int engine);

  /// Reads the PCM tap.
  @JS('_auris_read_pcm')
  external int readPcm(int engine, int out, int max, int outWritten);

  /// Takes the next event into a record.
  @JS('_auris_poll_event')
  external int pollEvent(int engine, int event);

  /// Takes the next packet into a record.
  @JS('_auris_drain')
  external int drain(int engine, int packet);

  /// Releases a drained packet slot.
  @JS('_auris_release')
  external int release(int engine, int packet);

  /// Reads the stats into a record.
  @JS('_auris_get_stats')
  external int getStats(int engine, int stats);

  /// Retunes a running engine.
  @JS('_auris_update_config')
  external int updateConfig(int engine, int cfg);

  /// The engine version as a C string.
  @JS('_auris_version_string')
  external int versionString();

  /// A status code's name as a C string.
  @JS('_auris_status_str')
  external int statusStr(int status);
}

/// The default export of `auris.js`: a factory for [AurisModule].
extension type AurisModuleFactory(JSFunction _) implements JSFunction {
  /// Instantiates the module with Emscripten's options object.
  external JSPromise<AurisModule> call(JSObject options);
}
