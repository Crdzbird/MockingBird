import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_config.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_pcm.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_queues.dart';
import 'package:ffi/ffi.dart';

/// The platform implementation over `dart:ffi`; a platform package supplies
/// the library that holds the C ABI.
abstract class AurisFfiPlatform extends AurisPlatform {
  /// The library containing the Auris C ABI.
  ffi.DynamicLibrary get nativeLibrary;

  /// The generated bindings, created once.
  AurisBindings get bindings => _bindings ??= AurisBindings(nativeLibrary);
  AurisBindings? _bindings;

  late final FfiQueues _queues = FfiQueues(bindings);
  late final FfiPcm _pcm = FfiPcm(bindings);

  @override
  String version() =>
      bindings.auris_version_string().cast<Utf8>().toDartString();

  @override
  String statusName(int status) =>
      bindings.auris_status_str(status).cast<Utf8>().toDartString();

  @override
  AurisCreateResult create(AurisNativeConfig config) {
    final native = FfiConfig.from(bindings, config);
    final out = calloc<ffi.Pointer<auris_engine_t>>();
    try {
      final s = bindings.auris_create(native.pointer, out).value;
      if (s != AurisStatusCode.ok) return (status: s, handle: null);
      return (status: s, handle: FfiEngineHandle(out.value));
    } finally {
      native.free();
      calloc.free(out);
    }
  }

  @override
  void destroy(AurisEngineHandle handle) {
    final h = handle as FfiEngineHandle;
    bindings.auris_destroy(h.pointer);
    h.free();
  }

  @override
  int start(AurisEngineHandle handle) =>
      bindings.auris_start(_ptr(handle)).value;

  @override
  int stop(AurisEngineHandle handle) =>
      bindings.auris_stop(_ptr(handle)).value;

  @override
  int pushPcm(AurisEngineHandle handle, Int16List samples, int captureTsUs) =>
      _pcm.push(handle as FfiEngineHandle, samples, captureTsUs);

  @override
  AurisPcmResult readPcm(AurisEngineHandle handle, int maxSamples) =>
      _pcm.read(handle as FfiEngineHandle, maxSamples);

  @override
  AurisEventResult pollEvent(AurisEngineHandle handle) =>
      _queues.pollEvent(handle as FfiEngineHandle);

  @override
  AurisPacketResult drainPacket(AurisEngineHandle handle) =>
      _queues.drainPacket(handle as FfiEngineHandle);

  @override
  AurisStatsResult stats(AurisEngineHandle handle) =>
      _queues.stats(handle as FfiEngineHandle);

  @override
  int updateConfig(AurisEngineHandle handle, AurisNativeConfig config) {
    final native = FfiConfig.from(bindings, config);
    try {
      return bindings.auris_update_config(_ptr(handle), native.pointer).value;
    } finally {
      native.free();
    }
  }

  static ffi.Pointer<auris_engine_t> _ptr(AurisEngineHandle h) =>
      (h as FfiEngineHandle).pointer;
}
