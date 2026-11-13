import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_web_base.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_decoder_handle.dart';
import 'package:mockingbird_web/src/web_opus_decoders.dart';

/// The standalone Opus decoder over WebAssembly.
abstract class MockingbirdWebDecoding extends MockingbirdWebBase {
  WebOpusDecoders get _decoders => WebOpusDecoders(ModuleRegistry.module);

  @override
  MockingbirdDecoderCreateResult createDecoder(
    int sampleRateHz,
    int channels,
  ) => _decoders.create(sampleRateHz, channels);

  @override
  void destroyDecoder(MockingbirdDecoderHandle handle) =>
      _decoders.destroy(handle as WebDecoderHandle);

  @override
  MockingbirdPcmResult decoderDecode(
    MockingbirdDecoderHandle handle,
    Uint8List packet,
  ) => _decoders.decode(handle as WebDecoderHandle, packet);

  @override
  MockingbirdPcmResult decoderConceal(
    MockingbirdDecoderHandle handle,
    int frames,
  ) => _decoders.conceal(handle as WebDecoderHandle, frames);

  @override
  int decoderReset(MockingbirdDecoderHandle handle) =>
      _decoders.reset(handle as WebDecoderHandle);
}
