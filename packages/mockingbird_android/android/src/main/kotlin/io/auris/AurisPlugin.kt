package io.auris

import io.flutter.embedding.engine.plugins.FlutterPlugin

/**
 * Minimal Flutter plugin registration. Auris does not use platform channels;
 * the Dart side reaches the C ABI directly via `dart:ffi` against the
 * dynamically-loaded `libauris.so`. This class exists only so the plugin is
 * recognised by the embedding and the AAR (which carries libauris.so) is
 * bundled into the app's APK.
 */
class AurisPlugin : FlutterPlugin {
    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        // Intentionally empty.
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        // Intentionally empty.
    }
}
