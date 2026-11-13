import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_decoder_handle.dart';

/// A native decoder pointer plus scratch memory for one packet in and the
/// longest Opus packet out (120 ms at 48 kHz, stereo).
final class FfiDecoderHandle extends MockingbirdDecoderHandle {
  /// Wraps [pointer] and allocates the scratch memory once.
  factory FfiDecoderHandle(
    ffi.Pointer<mockingbird_decoder_t> pointer, {
    required int sampleRateHz,
    required int channels,
  }) = FfiDecoderHandle._;

  FfiDecoderHandle._(
    this.pointer, {
    required int sampleRateHz,
    required this.channels,
  }) : maxFrames = sampleRateHz ~/ 1000 * 120,
       bytes = calloc(maxPacketBytes),
       pcm = calloc(sampleRateHz ~/ 1000 * 120 * channels),
       written = calloc();

  /// The largest packet the C ABI accepts.
  static const int maxPacketBytes = 65536;

  /// The decoder.
  final ffi.Pointer<mockingbird_decoder_t> pointer;

  /// Interleaved channels per frame.
  final int channels;

  /// Frames [pcm] holds.
  final int maxFrames;

  /// Scratch packet bytes.
  final ffi.Pointer<ffi.Uint8> bytes;

  /// Scratch output samples.
  final ffi.Pointer<ffi.Int16> pcm;

  /// Scratch frame count.
  final ffi.Pointer<ffi.Uint32> written;

  /// Frees the scratch memory; the decoder itself is destroyed separately.
  void free() {
    calloc
      ..free(bytes)
      ..free(pcm)
      ..free(written);
  }
}
