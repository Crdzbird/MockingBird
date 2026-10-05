import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/web_capture.dart';
import 'package:mockingbird_web/src/web_capture_worklet.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

/// Start and stop, including the microphone the platform drives itself,
/// and the events it reports when capture fails.
final class WebLifecycle {
  /// Creates the lifecycle over the module, pushing captured PCM through
  /// the platform's push.
  WebLifecycle(this._module, this._pushPcm);

  final AurisModule Function() _module;
  final int Function(WebEngineHandle, Int16List, int) _pushPcm;
  final List<AurisNativeEvent> _pending = [];

  /// Starts the engine and, for a capture engine, the microphone.
  int start(WebEngineHandle h) {
    final s = _module().start(h.pointer);
    if (s == AurisStatusCode.ok && h.captures) {
      final capture = WebCapture(
        h.sampleRateHz,
        (pcm, ts) => _pushPcm(h, pcm, ts),
      );
      h
        ..capture = capture
        ..captureStarted = capture.start().catchError(_failed);
    }
    return s;
  }

  /// Stops the microphone, then the engine.
  int stop(WebEngineHandle h) {
    h.capture?.stop();
    h.capture = null;
    return _module().stop(h.pointer);
  }

  /// The next capture failure to report, if any.
  AurisNativeEvent? takePending() =>
      _pending.isEmpty ? null : _pending.removeAt(0);

  void _failed(Object error) {
    _pending.add(const WebCaptureWorklet().errorEvent(error));
  }
}
