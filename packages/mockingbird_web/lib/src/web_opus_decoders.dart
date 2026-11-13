import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_decoder_api.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/web_decoder_handle.dart';

/// The standalone Opus decoder over the WebAssembly module.
final class WebOpusDecoders {
  /// Wraps the loaded module.
  const factory WebOpusDecoders(MockingbirdModule m) = WebOpusDecoders._;

  const WebOpusDecoders._(this._m);

  final MockingbirdModule _m;

  MockingbirdDecoderApi get _api => MockingbirdDecoderApi(_m);

  /// Creates a decoder.
  MockingbirdDecoderCreateResult create(int rate, int channels) {
    final out = _m.malloc(4);
    try {
      final s = _api.decoderCreate(rate, channels, out);
      if (s != MockingbirdStatusCode.ok) return (status: s, handle: null);
      final pointer = _m.heap32.toDart[out >> 2];
      return (status: s, handle: WebDecoderHandle(_m, pointer, rate, channels));
    } finally {
      _m.free(out);
    }
  }

  /// Destroys a decoder and frees its scratch memory.
  void destroy(WebDecoderHandle h) {
    _api.decoderDestroy(h.pointer);
    h.free(_m);
  }

  /// Decodes [packet].
  MockingbirdPcmResult decode(WebDecoderHandle h, Uint8List packet) {
    if (packet.isEmpty || packet.length > WebDecoderHandle.maxPacketBytes) {
      return (status: MockingbirdStatusCode.invalidArg, samples: Int16List(0));
    }
    _m.heapU8.toDart.setAll(h.bytes, packet);
    final s = _api.decoderDecode(
      h.pointer,
      h.bytes,
      packet.length,
      h.pcm,
      h.maxFrames,
      h.written,
    );
    return _copy(h, s);
  }

  /// Conceals one missing packet.
  MockingbirdPcmResult conceal(WebDecoderHandle h, int frames) => _copy(
    h,
    _api.decoderConceal(h.pointer, frames, h.pcm, h.maxFrames, h.written),
  );

  /// Forgets the history.
  int reset(WebDecoderHandle h) => _api.decoderReset(h.pointer);

  MockingbirdPcmResult _copy(WebDecoderHandle h, int s) {
    if (s != MockingbirdStatusCode.ok) {
      return (status: s, samples: Int16List(0));
    }
    final n = _m.heap32.toDart[h.written >> 2] * h.channels;
    final start = h.pcm >> 1;
    return (status: s, samples: _m.heap16.toDart.sublist(start, start + n));
  }
}
