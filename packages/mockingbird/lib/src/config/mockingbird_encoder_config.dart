import 'package:mockingbird/src/mockingbird_codec.dart';

/// How segment audio is encoded before it reaches the host.
final class MockingbirdEncoderConfig {
  /// Creates an encoder configuration.
  const MockingbirdEncoderConfig({
    this.codec = .pcm16,
    this.opusBitrateBps = 32000,
    this.opusVbr = true,
    this.opusComplexity = 5,
  });

  /// Opus at 32 kbps VBR, the usual choice for live voice.
  const MockingbirdEncoderConfig.opus() : this(codec: .opus);

  /// The packet encoding.
  final MockingbirdCodec codec;

  /// Opus target bitrate in bits per second.
  final int opusBitrateBps;

  /// Opus variable bitrate.
  final bool opusVbr;

  /// Opus complexity 0..10; higher is better quality for more CPU.
  final int opusComplexity;
}
