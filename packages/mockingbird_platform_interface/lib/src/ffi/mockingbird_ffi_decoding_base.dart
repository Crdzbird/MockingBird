import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_decoder_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_opus_decoders.dart';
import 'package:mockingbird_platform_interface/src/ffi/mockingbird_ffi_platform_base.dart';

/// The standalone Opus decoder operations over `dart:ffi`.
abstract class MockingbirdFfiDecodingBase extends MockingbirdFfiPlatformBase {
  late final FfiOpusDecoders _decoders = FfiOpusDecoders(bindings);

  @override
  MockingbirdDecoderCreateResult createDecoder(
    int sampleRateHz,
    int channels,
  ) => _decoders.create(sampleRateHz, channels);

  @override
  void destroyDecoder(MockingbirdDecoderHandle handle) =>
      _decoders.destroy(handle as FfiDecoderHandle);

  @override
  MockingbirdPcmResult decoderDecode(
    MockingbirdDecoderHandle handle,
    Uint8List packet,
  ) => _decoders.decode(handle as FfiDecoderHandle, packet);

  @override
  MockingbirdPcmResult decoderConceal(
    MockingbirdDecoderHandle handle,
    int frames,
  ) => _decoders.conceal(handle as FfiDecoderHandle, frames);

  @override
  int decoderReset(MockingbirdDecoderHandle handle) =>
      _decoders.reset(handle as FfiDecoderHandle);
}
