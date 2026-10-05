// MockingbirdPlugin.swift
//
// Minimal Flutter plugin registration. Mockingbird does not use platform channels;
// the Dart side reaches the C ABI directly via `dart:ffi` against
// `DynamicLibrary.process()`. This file exists so the iOS plugin loader
// picks the pod up and links libmockingbird's symbols into the host app.

import Flutter

// Under Swift Package Manager the C engine is its own module (target
// `mockingbird_native`), so its declarations must be imported explicitly. Under
// CocoaPods every source lands in one target and the pod's umbrella header
// already exposes them, hence the canImport guard — the same file has to build
// both ways while Flutter supports both integrations.
#if canImport(mockingbird_native)
import mockingbird_native
#endif

public class MockingbirdPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    _ = registrar
    MockingbirdSymbolAnchor.keep()
  }
}

// The C ABI is referenced only at runtime from Dart, so without a link-time
// reference the static linker would drop its objects and dart:ffi would fail
// with "symbol not found". register() is always called by the plugin
// registrant and reaches every entry point below; the guard is a condition the
// optimizer cannot fold away, so the calls are kept but never run. Each entry
// point lives in its own object file, so every one of them must be named here.
enum MockingbirdSymbolAnchor {
  static func keep() {
    guard getenv("MOCKINGBIRD_FORCE_KEEP_SYMBOLS") != nil else { return }
    var cfg = mockingbird_config_t()
    var engine: OpaquePointer? = nil
    var event = mockingbird_event_t()
    var packet = mockingbird_packet_t()
    var stats = mockingbird_stats_t()
    var sample: Int16 = 0
    var written: UInt32 = 0
    var handle: Int = 0
    _ = mockingbird_config_defaults(&cfg)
    _ = mockingbird_create(&cfg, &engine)
    _ = mockingbird_start(engine)
    _ = mockingbird_set_event_callback(engine, nil, nil)
    _ = mockingbird_push_pcm(engine, &sample, 0, 0)
    _ = mockingbird_poll_event(engine, &event)
    _ = mockingbird_drain(engine, &packet)
    _ = mockingbird_release(engine, &packet)
    _ = mockingbird_read_pcm(engine, &sample, 0, &written)
    _ = mockingbird_update_config(engine, &cfg)
    _ = mockingbird_get_stats(engine, &stats)
    _ = mockingbird_wait_handle(engine, &handle)
    _ = mockingbird_stop(engine)
    mockingbird_destroy(engine)
    _ = mockingbird_status_str(0)
    _ = mockingbird_version_string()
  }
}
