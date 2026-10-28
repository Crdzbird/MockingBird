import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'package:web/web.dart' as web;

/// Turns the engine's voice-processing bits and device choice into
/// `getUserMedia` constraints, and reads back what the browser applied.
abstract final class WebCaptureConstraints {
  static const _echo = 1;
  static const _gain = 2;
  static const _noise = 4;

  /// Mono capture with the [voice] bits on and, if set, [deviceId] exactly.
  static web.MediaStreamConstraints audio(int voice, String? deviceId) {
    final track = web.MediaTrackConstraints(
      echoCancellation: (voice & _echo != 0).toJS,
      autoGainControl: (voice & _gain != 0).toJS,
      noiseSuppression: (voice & _noise != 0).toJS,
      channelCount: 1.toJS,
    );
    if (deviceId != null) {
      track.deviceId = JSObject()..setProperty('exact'.toJS, deviceId.toJS);
    }
    return web.MediaStreamConstraints(audio: track);
  }

  /// The voice-processing bits the browser reports for [stream]'s track.
  static int applied(web.MediaStream stream) {
    final tracks = stream.getAudioTracks().toDart;
    if (tracks.isEmpty) return 0;
    final settings = tracks.first.getSettings() as JSObject;
    bool on(String key) =>
        settings.getProperty<JSBoolean?>(key.toJS)?.toDart ?? false;
    return (on('echoCancellation') ? _echo : 0) |
        (on('autoGainControl') ? _gain : 0) |
        (on('noiseSuppression') ? _noise : 0);
  }
}
