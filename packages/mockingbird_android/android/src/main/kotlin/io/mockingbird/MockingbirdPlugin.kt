package io.mockingbird

import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

/**
 * Flutter plugin registration. The engine, the decoder and the player are
 * reached directly through `dart:ffi` against `libmockingbird.so`, which the
 * AAR carries; the only channel is for audio focus, a Java API with no NDK
 * counterpart, and it is used once per player start and stop.
 */
class MockingbirdPlugin : FlutterPlugin, MethodChannel.MethodCallHandler, EventChannel.StreamHandler {
    private var methods: MethodChannel? = null
    private var events: EventChannel? = null
    private var focus: MockingbirdAudioFocus? = null

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        focus = MockingbirdAudioFocus(binding.applicationContext)
        methods = MethodChannel(binding.binaryMessenger, "io.mockingbird/playback").also {
            it.setMethodCallHandler(this)
        }
        events = EventChannel(binding.binaryMessenger, "io.mockingbird/playback_focus").also {
            it.setStreamHandler(this)
        }
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        focus?.release()
        focus = null
        methods?.setMethodCallHandler(null)
        events?.setStreamHandler(null)
        methods = null
        events = null
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        val f = focus ?: return result.error("detached", "The plugin is detached.", null)
        when (call.method) {
            "requestFocus" -> result.success(f.request())
            "abandonFocus" -> {
                f.abandon()
                result.success(null)
            }
            else -> result.notImplemented()
        }
    }

    override fun onListen(arguments: Any?, sink: EventChannel.EventSink?) {
        focus?.listen(sink)
    }

    override fun onCancel(arguments: Any?) {
        focus?.listen(null)
    }
}
