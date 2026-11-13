import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_decoder_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_platform_playback.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The standalone Opus decoder operations; each default says it cannot.
abstract class MockingbirdPlatformDecoding extends MockingbirdPlatformPlayback {
  /// Passes the verification [token] up.
  MockingbirdPlatformDecoding({required super.token});

  /// Creates a decoder that outputs [sampleRateHz] with [channels].
  MockingbirdDecoderCreateResult createDecoder(
    int sampleRateHz,
    int channels,
  ) => (status: MockingbirdStatusCode.unsupported, handle: null);

  /// Destroys a decoder.
  void destroyDecoder(MockingbirdDecoderHandle handle) {}

  /// Decodes one Opus packet into interleaved samples.
  MockingbirdPcmResult decoderDecode(
    MockingbirdDecoderHandle handle,
    Uint8List packet,
  ) => (status: MockingbirdStatusCode.unsupported, samples: Int16List(0));

  /// Conceals one missing packet of [frames] frames; 0 repeats the last
  /// packet's length.
  MockingbirdPcmResult decoderConceal(
    MockingbirdDecoderHandle handle,
    int frames,
  ) => (status: MockingbirdStatusCode.unsupported, samples: Int16List(0));

  /// Forgets the decoder's history.
  int decoderReset(MockingbirdDecoderHandle handle) =>
      MockingbirdStatusCode.unsupported;
}
