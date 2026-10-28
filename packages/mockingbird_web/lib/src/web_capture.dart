import 'dart:js_interop';
import 'dart:js_interop_unsafe';
import 'dart:typed_data';

import 'package:mockingbird_web/src/web_capture_constraints.dart';
import 'package:mockingbird_web/src/web_capture_worklet.dart';
import 'package:web/web.dart' as web;

/// Microphone capture through an AudioWorklet that delivers int16 chunks at
/// the engine's sample rate.
final class WebCapture {
  /// Creates capture at [sampleRateHz] delivering to [onPcm], asking the
  /// browser for the [voice] bits and, when set, the microphone [deviceId].
  factory WebCapture(
    int sampleRateHz,
    void Function(Int16List pcm, int captureTsUs) onPcm, {
    int voice,
    String? deviceId,
  }) = WebCapture._;

  WebCapture._(this.sampleRateHz, this.onPcm, {this.voice = 0, this.deviceId});

  /// The rate the engine expects.
  final int sampleRateHz;

  /// The voice-processing bits asked for.
  final int voice;

  /// The microphone asked for, or null for the default.
  final String? deviceId;

  /// The voice-processing bits the browser applied once capture started.
  int voiceActive = 0;

  /// Receives each chunk with the capture timestamp of its first sample.
  final void Function(Int16List pcm, int captureTsUs) onPcm;

  web.MediaStream? _stream;
  web.AudioContext? _context;
  web.AudioWorkletNode? _node;

  /// Requests the microphone and starts delivering chunks.
  Future<void> start() async {
    final stream = await web.window.navigator.mediaDevices
        .getUserMedia(WebCaptureConstraints.audio(voice, deviceId))
        .toDart;
    voiceActive = WebCaptureConstraints.applied(stream);
    final context = web.AudioContext(
      web.AudioContextOptions(sampleRate: sampleRateHz),
    );
    await const WebCaptureWorklet().add(context);
    final node = web.AudioWorkletNode(
      context,
      'mockingbird-capture',
      web.AudioWorkletNodeOptions(
        processorOptions: _options(context.sampleRate.toInt()),
      ),
    );
    node.port.onmessage = _onMessage.toJS;
    context.createMediaStreamSource(stream).connect(node);
    _stream = stream;
    _context = context;
    _node = node;
  }

  JSObject _options(int contextRate) => JSObject()
    ..setProperty('inputRate'.toJS, contextRate.toJS)
    ..setProperty('outputRate'.toJS, sampleRateHz.toJS)
    ..setProperty('chunk'.toJS, (sampleRateHz ~/ 50).toJS);

  void _onMessage(web.MessageEvent e) {
    final pcm = (e.data! as JSInt16Array).toDart;
    final nowUs = (web.window.performance.now() * 1000).toInt();
    final chunkUs = pcm.length * 1000000 ~/ sampleRateHz;
    onPcm(pcm, nowUs > chunkUs ? nowUs - chunkUs : 0);
  }

  /// Stops delivering and releases the microphone.
  void stop() {
    _node?.disconnect();
    _stream?.getTracks().toDart.forEach((t) => t.stop());
    _context?.close();
    _node = null;
    _stream = null;
    _context = null;
  }
}
