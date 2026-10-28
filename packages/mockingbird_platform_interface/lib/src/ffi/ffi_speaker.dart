import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';

/// Speaker verification over the FFI bindings.
final class FfiSpeaker {
  /// Wraps [_bindings].
  const factory FfiSpeaker(MockingbirdBindings bindings) = FfiSpeaker._;

  const FfiSpeaker._(this._bindings);

  static const int _capacity = 4096;

  final MockingbirdBindings _bindings;

  /// Names the speaker embedding model; null uses the default.
  int model(FfiEngineHandle h, String? path) => using(
    (arena) => _bindings
        .mockingbird_set_speaker_model(
          h.pointer,
          path?.toNativeUtf8(allocator: arena).cast() ?? nullptr,
        )
        .value,
  );

  /// The voice print of [samples].
  MockingbirdSpeakerEmbeddingResult embedding(
    FfiEngineHandle h,
    Int16List samples,
  ) => using((arena) {
    final input = arena<Int16>(samples.isEmpty ? 1 : samples.length);
    input.asTypedList(samples.length).setAll(0, samples);
    final out = arena<Float>(_capacity);
    final dim = arena<Uint32>();
    final s = _bindings
        .mockingbird_speaker_embedding(
          h.pointer,
          input,
          samples.length,
          out,
          _capacity,
          dim,
        )
        .value;
    final n = dim.value < _capacity ? dim.value : _capacity;
    return (status: s, embedding: Float32List.fromList(out.asTypedList(n)));
  });

  /// Sets the target voice print; null stops.
  int target(FfiEngineHandle h, Float32List? embedding, double threshold) =>
      using((arena) {
        final n = embedding?.length ?? 0;
        final p = embedding == null ? nullptr : arena<Float>(n);
        if (embedding != null) p.asTypedList(n).setAll(0, embedding);
        return _bindings
            .mockingbird_set_target_speaker(h.pointer, p, n, threshold)
            .value;
      });
}
