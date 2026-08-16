// AurisPlugin.swift (macOS)
//
// Minimal Flutter plugin registration. Auris does not use platform channels;
// the Dart side reaches the C ABI directly via `dart:ffi` against
// `DynamicLibrary.process()`. This file exists so the macOS plugin loader
// picks the pod up and links libauris's symbols into the host app.

import FlutterMacOS

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

// libauris's C ABI is referenced only at runtime from Dart (dart:ffi against
// DynamicLibrary.process()), so the static linker, seeing no link-time
// reference, would drop the whole archive and the lookup would fail with
// "Failed to lookup symbol 'auris_*': symbol not found".
//
// register() (always invoked by Flutter's plugin registrant) calls into the
// ABI below, which anchors the entire transitive closure — engine, detectors,
// preprocessor, encoders, and the vendored C deps — into the host binary. The
// guard is a condition the optimizer cannot fold away, so the calls are
// retained but never actually run. The podspec's -export_dynamic then publishes
// these (default-visibility) symbols into the executable's dynamic symbol table
// so DynamicLibrary.process() can resolve them.
enum AurisSymbolAnchor {
  static func keep() {
    guard getenv("AURIS_FORCE_KEEP_SYMBOLS") != nil else { return }
    var cfg = auris_config_t()
    _ = auris_config_defaults(&cfg)   // pulls engine.o + its transitive graph
    _ = auris_status_str(0)           // pulls status.o
    _ = auris_version_string()
  }
}
