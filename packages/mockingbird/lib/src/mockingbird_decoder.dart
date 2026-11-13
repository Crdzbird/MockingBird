import 'dart:typed_data';

import 'package:mockingbird/src/mockingbird_exception.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Decodes the Opus packets of one stream into int16 PCM, in order, with
/// concealment for a packet that never arrived. One decoder per stream.
final class MockingbirdDecoder {
  MockingbirdDecoder._(this._platform, this._handle, this.sampleRateHz);

  /// Creates a decoder that outputs [sampleRateHz] (8000, 12000, 16000,
  /// 24000 or 48000) with [channels] (1 or 2).
  factory MockingbirdDecoder.create({
    int sampleRateHz = 16000,
    int channels = 1,
  }) {
    final platform = MockingbirdPlatform.instance;
    final r = platform.createDecoder(sampleRateHz, channels);
    MockingbirdException.check(r.status, 'mockingbird_decoder_create');
    return MockingbirdDecoder._(platform, r.handle!, sampleRateHz);
  }

  /// The output rate.
  final int sampleRateHz;

  final MockingbirdPlatform _platform;
  final MockingbirdDecoderHandle _handle;
  bool _disposed = false;

  /// Decodes one packet; interleaved when stereo.
  Int16List decode(Uint8List packet) {
    _ensureLive();
    final r = _platform.decoderDecode(_handle, packet);
    MockingbirdException.check(r.status, 'mockingbird_decoder_decode');
    return r.samples;
  }

  /// Audio for one missing packet of [frames] frames; 0 repeats the last
  /// packet's length.
  Int16List conceal({int frames = 0}) {
    _ensureLive();
    final r = _platform.decoderConceal(_handle, frames);
    MockingbirdException.check(r.status, 'mockingbird_decoder_conceal');
    return r.samples;
  }

  /// Forgets the history, for a new stream.
  void reset() {
    _ensureLive();
    MockingbirdException.check(
      _platform.decoderReset(_handle),
      'mockingbird_decoder_reset',
    );
  }

  /// Destroys the native decoder. Idempotent.
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    _platform.destroyDecoder(_handle);
  }

  void _ensureLive() {
    if (_disposed) throw StateError('MockingbirdDecoder has been disposed.');
  }
}
