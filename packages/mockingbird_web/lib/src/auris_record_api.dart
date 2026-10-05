import 'dart:js_interop';

/// The shim's record entry points: configs, events, packets and stats are
/// built and read through numbered fields, so no struct layout is mirrored.
extension type AurisRecordApi(JSObject _) implements JSObject {
  /// Allocates a config filled with the engine defaults.
  @JS('_auris_web_config_new')
  external int configNew();

  /// Frees a config.
  @JS('_auris_web_config_free')
  external void configFree(int cfg);

  /// Number of config fields the shim knows.
  @JS('_auris_web_config_field_count')
  external int configFieldCount();

  /// Sets one config field by id.
  @JS('_auris_web_config_set')
  external int configSet(int cfg, int field, double value);

  /// Creates an engine; the status lands at [outStatus].
  @JS('_auris_web_create')
  external int createEngine(int cfg, int outStatus);

  /// Pushes int16 samples with a capture timestamp.
  @JS('_auris_web_push_pcm')
  external int pushPcm(int engine, int samples, int count, double ts);

  /// Allocates an event record.
  @JS('_auris_web_event_new')
  external int eventNew();

  /// Frees an event record.
  @JS('_auris_web_event_free')
  external void eventFree(int event);

  /// Reads one event field by id.
  @JS('_auris_web_event_get')
  external double eventGet(int event, int field);

  /// Allocates a packet record.
  @JS('_auris_web_packet_new')
  external int packetNew();

  /// Frees a packet record.
  @JS('_auris_web_packet_free')
  external void packetFree(int packet);

  /// Reads one packet field by id.
  @JS('_auris_web_packet_get')
  external double packetGet(int packet, int field);

  /// Allocates a stats record.
  @JS('_auris_web_stats_new')
  external int statsNew();

  /// Frees a stats record.
  @JS('_auris_web_stats_free')
  external void statsFree(int stats);

  /// Reads one stats field by id.
  @JS('_auris_web_stats_get')
  external double statsGet(int stats, int field);
}
