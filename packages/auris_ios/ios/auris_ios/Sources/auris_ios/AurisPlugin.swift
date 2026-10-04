// AurisPlugin.swift
//
// Minimal Flutter plugin registration. Auris does not use platform channels;
// the Dart side reaches the C ABI directly via `dart:ffi` against
// `DynamicLibrary.process()`. This file exists so the iOS plugin loader
// picks the pod up and links libauris's symbols into the host app.

import Flutter

// Under Swift Package Manager the C engine is its own module (target
// `auris_native`), so its declarations must be imported explicitly. Under
// CocoaPods every source lands in one target and the pod's umbrella header
// already exposes them, hence the canImport guard — the same file has to build
// both ways while Flutter supports both integrations.
#if canImport(auris_native)
import auris_native
#endif

public class AurisPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    _ = registrar
    AurisSymbolAnchor.keep()
  }
}

// The C ABI is referenced only at runtime from Dart, so without a link-time
// reference the static linker would drop its objects and dart:ffi would fail
// with "symbol not found". register() is always called by the plugin
// registrant and reaches every entry point below; the guard is a condition the
// optimizer cannot fold away, so the calls are kept but never run. Each entry
// point lives in its own object file, so every one of them must be named here.
enum AurisSymbolAnchor {
  static func keep() {
    guard getenv("AURIS_FORCE_KEEP_SYMBOLS") != nil else { return }
    var cfg = auris_config_t()
    var engine: OpaquePointer? = nil
    var event = auris_event_t()
    var packet = auris_packet_t()
    var stats = auris_stats_t()
    var sample: Int16 = 0
    var written: UInt32 = 0
    var handle: Int = 0
    _ = auris_config_defaults(&cfg)
    _ = auris_create(&cfg, &engine)
    _ = auris_start(engine)
    _ = auris_set_event_callback(engine, nil, nil)
    _ = auris_push_pcm(engine, &sample, 0, 0)
    _ = auris_poll_event(engine, &event)
    _ = auris_drain(engine, &packet)
    _ = auris_release(engine, &packet)
    _ = auris_read_pcm(engine, &sample, 0, &written)
    _ = auris_update_config(engine, &cfg)
    _ = auris_get_stats(engine, &stats)
    _ = auris_wait_handle(engine, &handle)
    _ = auris_stop(engine)
    auris_destroy(engine)
    _ = auris_status_str(0)
    _ = auris_version_string()
  }
}
