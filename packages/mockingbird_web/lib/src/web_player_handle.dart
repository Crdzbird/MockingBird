import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/web_audio_output.dart';

/// A WebAssembly player pointer, its scratch memory and its Web Audio
/// output. A host-PCM config gets no output: the app reads it itself.
final class WebPlayerHandle extends MockingbirdPlayerHandle {
  /// Allocates the scratch memory for [pointer], created from [config].
  factory WebPlayerHandle(
    MockingbirdModule m,
    int pointer,
    MockingbirdNativePlayerConfig config,
  ) = WebPlayerHandle._;

  WebPlayerHandle._(
    MockingbirdModule m,
    this.pointer,
    MockingbirdNativePlayerConfig c,
  ) : channels = c.channels,
      sampleRateHz = c.sampleRateHz,
      plays = c.backend != _hostPcm,
      bytes = m.malloc(maxPacketBytes),
      pcm = m.malloc(c.sampleRateHz * c.channels * 2);

  static const _hostPcm = 4;

  /// The largest packet the C ABI accepts.
  static const int maxPacketBytes = 8192;

  /// The player in linear memory.
  final int pointer;

  /// Interleaved channels.
  final int channels;

  /// The player's rate.
  final int sampleRateHz;

  /// Whether the platform plays the output through Web Audio.
  final bool plays;

  /// Scratch packet bytes.
  final int bytes;

  /// Scratch output, one second.
  final int pcm;

  /// The Web Audio output while started.
  WebAudioOutput? output;

  /// Frees the scratch memory.
  void free(MockingbirdModule m) {
    m
      ..free(bytes)
      ..free(pcm);
  }
}
