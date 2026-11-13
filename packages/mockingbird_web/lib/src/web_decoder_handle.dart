import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';

/// A WebAssembly decoder pointer and scratch memory for one packet in and
/// the longest Opus packet out.
final class WebDecoderHandle extends MockingbirdDecoderHandle {
  /// Allocates the scratch memory for [pointer].
  factory WebDecoderHandle(
    MockingbirdModule m,
    int pointer,
    int sampleRateHz,
    int channels,
  ) = WebDecoderHandle._;

  WebDecoderHandle._(MockingbirdModule m, this.pointer, int rate, this.channels)
    : maxFrames = rate ~/ 1000 * 120,
      bytes = m.malloc(maxPacketBytes),
      pcm = m.malloc(rate ~/ 1000 * 120 * channels * 2),
      written = m.malloc(4);

  /// The largest packet the C ABI accepts.
  static const int maxPacketBytes = 65536;

  /// The decoder in linear memory.
  final int pointer;

  /// Interleaved channels.
  final int channels;

  /// Frames [pcm] holds.
  final int maxFrames;

  /// Scratch packet bytes.
  final int bytes;

  /// Scratch output.
  final int pcm;

  /// Scratch frame count.
  final int written;

  /// Frees the scratch memory.
  void free(MockingbirdModule m) {
    m
      ..free(bytes)
      ..free(pcm)
      ..free(written);
  }
}
