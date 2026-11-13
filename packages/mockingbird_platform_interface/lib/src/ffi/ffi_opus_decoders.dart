import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_decoder_handle.dart';

/// The standalone Opus decoder over the FFI bindings.
final class FfiOpusDecoders {
  /// Wraps [_bindings].
  const factory FfiOpusDecoders(MockingbirdBindings bindings) =
      FfiOpusDecoders._;

  const FfiOpusDecoders._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Creates a decoder.
  MockingbirdDecoderCreateResult create(int sampleRateHz, int channels) {
    final out = calloc<ffi.Pointer<mockingbird_decoder_t>>();
    try {
      final s = _bindings
          .mockingbird_decoder_create(sampleRateHz, channels, out)
          .value;
      if (s != MockingbirdStatusCode.ok) return (status: s, handle: null);
      final h = FfiDecoderHandle(
        out.value,
        sampleRateHz: sampleRateHz,
        channels: channels,
      );
      return (status: s, handle: h);
    } finally {
      calloc.free(out);
    }
  }

  /// Destroys the decoder and frees its scratch memory.
  void destroy(FfiDecoderHandle h) {
    _bindings.mockingbird_decoder_destroy(h.pointer);
    h.free();
  }

  /// Decodes [packet].
  MockingbirdPcmResult decode(FfiDecoderHandle h, Uint8List packet) {
    if (packet.isEmpty || packet.length > FfiDecoderHandle.maxPacketBytes) {
      return (status: MockingbirdStatusCode.invalidArg, samples: Int16List(0));
    }
    h.bytes.asTypedList(packet.length).setAll(0, packet);
    final s = _bindings
        .mockingbird_decoder_decode(
          h.pointer,
          h.bytes,
          packet.length,
          h.pcm,
          h.maxFrames,
          h.written,
        )
        .value;
    return _copy(h, s);
  }

  /// Conceals one missing packet.
  MockingbirdPcmResult conceal(FfiDecoderHandle h, int frames) {
    final s = _bindings
        .mockingbird_decoder_conceal(
          h.pointer,
          frames,
          h.pcm,
          h.maxFrames,
          h.written,
        )
        .value;
    return _copy(h, s);
  }

  /// Forgets the history.
  int reset(FfiDecoderHandle h) =>
      _bindings.mockingbird_decoder_reset(h.pointer).value;

  MockingbirdPcmResult _copy(FfiDecoderHandle h, int s) {
    if (s != MockingbirdStatusCode.ok) {
      return (status: s, samples: Int16List(0));
    }
    final n = h.written.value * h.channels;
    return (status: s, samples: Int16List.fromList(h.pcm.asTypedList(n)));
  }
}
