@TestOn('browser')
library;

import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird_web/src/wasm_features.dart';

void main() {
  test(
    'Chrome runs WebAssembly SIMD, so the SIMD module is the one loaded',
    () {
      expect(WasmFeatures.simd, isTrue);
    },
  );
}
