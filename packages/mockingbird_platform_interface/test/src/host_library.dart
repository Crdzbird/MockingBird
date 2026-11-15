import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';

/// The library built at the repository root, for tests on the host.
abstract final class HostLibrary {
  /// The library's path under the repository's `build/`.
  static String get path {
    final name = Platform.isMacOS
        ? 'libmockingbird.dylib'
        : Platform.isWindows
        ? 'mockingbird.dll'
        : 'libmockingbird.so';
    return '${Directory.current.path}/../../build/$name';
  }

  /// Whether the library has been built.
  static bool get available => File(path).existsSync();

  /// Bindings over the library.
  static MockingbirdBindings bindings() =>
      MockingbirdBindings(DynamicLibrary.open(path));

  /// A started host-PCM engine with no detectors, so every frame is a packet.
  static Pointer<mockingbird_engine_t> startedEngine(MockingbirdBindings b) {
    final cfg = calloc<mockingbird_config_t>();
    final out = calloc<Pointer<mockingbird_engine_t>>();
    try {
      b.mockingbird_config_defaults(cfg);
      cfg.ref
        ..backendAsInt = mockingbird_backend.MOCKINGBIRD_BACKEND_HOST_PCM.value
        ..enable_vad = 0
        ..enable_anomaly = 0;
      b.mockingbird_create(cfg, out);
      final engine = out.value;
      b.mockingbird_start(engine);
      return engine;
    } finally {
      calloc
        ..free(cfg)
        ..free(out);
    }
  }

  /// Pushes [frames] frames of 20 ms at 16 kHz.
  static void push(
    MockingbirdBindings b,
    Pointer<mockingbird_engine_t> e,
    int frames,
  ) {
    final pcm = calloc<Int16>(320);
    try {
      for (var i = 0; i < frames; i++) {
        b.mockingbird_push_pcm(e, pcm, 320, i * 20000);
      }
    } finally {
      calloc.free(pcm);
    }
  }
}
