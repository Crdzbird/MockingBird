import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';

/// Keyword spotting setup over the FFI bindings.
final class FfiKeywords {
  /// Wraps [_bindings].
  const factory FfiKeywords(MockingbirdBindings bindings) = FfiKeywords._;

  const FfiKeywords._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Names the two front-end models; null uses the engine's defaults.
  int frontend(FfiEngineHandle h, String? melspectrogram, String? embedding) =>
      using((arena) {
        final mel = melspectrogram?.toNativeUtf8(allocator: arena);
        final emb = embedding?.toNativeUtf8(allocator: arena);
        return _bindings
            .mockingbird_set_keyword_frontend(
              h.pointer,
              mel?.cast() ?? nullptr,
              emb?.cast() ?? nullptr,
            )
            .value;
      });

  /// Registers the keyword model at [path].
  MockingbirdKeywordResult add(
    FfiEngineHandle h,
    String path,
    double threshold,
  ) => using((arena) {
    final id = arena<Uint32>();
    final s = _bindings
        .mockingbird_add_keyword(
          h.pointer,
          path.toNativeUtf8(allocator: arena).cast(),
          threshold,
          id,
        )
        .value;
    return (status: s, id: id.value);
  });

  /// Removes every keyword.
  int clear(FfiEngineHandle h) =>
      _bindings.mockingbird_clear_keywords(h.pointer).value;
}
