import 'dart:async';
import 'dart:js_interop';
import 'dart:typed_data';

import 'package:web/web.dart' as web;

/// Plays what [pull] renders through Web Audio: every 20 ms it schedules
/// buffers of the player's output back to back, 120 ms ahead of the
/// context's clock, so the page's own timers set the pace of playout.
final class WebAudioOutput {
  /// Creates an output for [sampleRateHz] and [channels].
  factory WebAudioOutput({
    required int sampleRateHz,
    required int channels,
    required Int16List Function(int frames) pull,
  }) = WebAudioOutput._;

  WebAudioOutput._({
    required this.sampleRateHz,
    required this.channels,
    required this.pull,
  });

  /// The player's rate; Web Audio resamples to the device.
  final int sampleRateHz;

  /// Interleaved channels.
  final int channels;

  /// Renders the next frames.
  final Int16List Function(int frames) pull;

  static const _lead = 0.12;
  static const _tick = Duration(milliseconds: 20);
  static const _grace = Duration(milliseconds: 200);

  web.AudioContext? _context;
  Timer? _timer;
  double _next = 0;

  /// Resumes the context and starts scheduling. Browsers allow audio only
  /// after a user gesture; without one the context stays suspended, its
  /// clock stands still and nothing plays until a later call from a gesture.
  Future<void> start() async {
    final context = _context ??= web.AudioContext();
    await Future.any([context.resume().toDart, Future<void>.delayed(_grace)]);
    _next = context.currentTime + 0.05;
    _timer ??= Timer.periodic(_tick, (_) => _schedule());
    _schedule();
  }

  /// Stops scheduling and suspends the context.
  Future<void> pause() async {
    _timer?.cancel();
    _timer = null;
    await _context?.suspend().toDart;
  }

  /// Stops scheduling and closes the context.
  Future<void> close() async {
    _timer?.cancel();
    _timer = null;
    final context = _context;
    _context = null;
    if (context != null) await context.close().toDart;
  }

  void _schedule() {
    final context = _context;
    if (context == null) return;
    if (_next < context.currentTime) _next = context.currentTime + 0.02;
    final frames = sampleRateHz ~/ 50;
    while (_next < context.currentTime + _lead) {
      final buffer = context.createBuffer(channels, frames, sampleRateHz);
      final pcm = pull(frames);
      for (var c = 0; c < channels; c++) {
        final data = Float32List(frames);
        for (var i = 0; i < frames && i * channels + c < pcm.length; i++) {
          data[i] = pcm[i * channels + c] / 32768;
        }
        buffer.copyToChannel(data.toJS, c);
      }
      context.createBufferSource()
        ..buffer = buffer
        ..connect(context.destination)
        ..start(_next);
      _next += frames / sampleRateHz;
    }
  }
}
