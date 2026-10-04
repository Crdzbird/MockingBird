import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:auris/src/auris_exception.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';

/// Moves 16-bit PCM across the native boundary, copying both ways so no
/// Dart object ever aliases engine memory.
final class NativePcm {
  /// Binds to one engine handle.
  const NativePcm(this._bindings, this._engine);

  final AurisBindings _bindings;
  final ffi.Pointer<auris_engine_t> _engine;

  /// Drains up to [maxSamples] from the PCM tap.
  Int16List read(int maxSamples) {
    final buf = calloc<ffi.Int16>(maxSamples);
    final written = calloc<ffi.Uint32>();
    try {
      AurisException.check(
        _bindings.auris_read_pcm(_engine, buf, maxSamples, written),
        'auris_read_pcm',
      );
      return Int16List.fromList(buf.asTypedList(written.value));
    } finally {
      calloc
        ..free(buf)
        ..free(written);
    }
  }

  /// Pushes [samples] into a host-PCM engine.
  void push(Int16List samples, int captureTsUs) {
    final buf = calloc<ffi.Int16>(samples.length);
    try {
      buf.asTypedList(samples.length).setAll(0, samples);
      AurisException.check(
        _bindings.auris_push_pcm(_engine, buf, samples.length, captureTsUs),
        'auris_push_pcm',
      );
    } finally {
      calloc.free(buf);
    }
  }
}
