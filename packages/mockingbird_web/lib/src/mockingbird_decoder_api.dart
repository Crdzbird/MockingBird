import 'dart:js_interop';

/// The module's standalone Opus decoder entry points.
extension type MockingbirdDecoderApi(JSObject _) implements JSObject {
  /// Creates a decoder; the pointer lands at [out].
  @JS('_mockingbird_decoder_create')
  external int decoderCreate(int rate, int channels, int out);

  /// Destroys a decoder.
  @JS('_mockingbird_decoder_destroy')
  external void decoderDestroy(int decoder);

  /// Decodes one packet.
  @JS('_mockingbird_decoder_decode')
  external int decoderDecode(
    int decoder,
    int data,
    int size,
    int out,
    int maxFrames,
    int outFrames,
  );

  /// Conceals one missing packet.
  @JS('_mockingbird_decoder_conceal')
  external int decoderConceal(
    int decoder,
    int frames,
    int out,
    int maxFrames,
    int outFrames,
  );

  /// Forgets the decoder's history.
  @JS('_mockingbird_decoder_reset')
  external int decoderReset(int decoder);
}
