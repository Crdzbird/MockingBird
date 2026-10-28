import 'dart:js_interop';
import 'dart:typed_data';

@JS('WebAssembly.validate')
external bool _validate(JSUint8Array bytes);

/// What the browser's WebAssembly engine supports.
abstract final class WasmFeatures {
  /// A module whose only function splats an i32 into a v128, so it
  /// validates exactly when the engine has fixed-width SIMD.
  static final Uint8List _simdProbe = Uint8List.fromList(const [
    0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, //
    10, 8, 1, 6, 0, 65, 0, 253, 15, 11,
  ]);

  /// True when the engine runs WebAssembly SIMD: Chrome 91, Firefox 89 and
  /// Safari 16.4 or later.
  static final bool simd = _validate(_simdProbe.toJS);
}
