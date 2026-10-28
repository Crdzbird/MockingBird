import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engines.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_pcm.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_queues.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_wake.dart';

/// The `dart:ffi` implementation; a platform package supplies the library.
abstract class MockingbirdFfiPlatform extends MockingbirdPlatform {
  /// The library containing the Mockingbird C ABI.
  ffi.DynamicLibrary get nativeLibrary;

  /// The generated bindings, created once.
  MockingbirdBindings get bindings =>
      _bindings ??= MockingbirdBindings(nativeLibrary);
  MockingbirdBindings? _bindings;

  late final FfiEngines _engines = FfiEngines(bindings);
  late final FfiQueues _queues = FfiQueues(bindings);
  late final FfiPcm _pcm = FfiPcm(bindings);
  late final FfiWake _wake = FfiWake(bindings);

  @override
  String version() => _engines.version();

  @override
  String statusName(int status) => _engines.statusName(status);

  @override
  MockingbirdCreateResult create(MockingbirdNativeConfig config) =>
      _engines.create(config);

  @override
  void destroy(MockingbirdEngineHandle handle) {
    final h = handle as FfiEngineHandle;
    if (h.wake != null) _wake.set(h, null);
    _engines.destroy(h);
  }

  @override
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) => _wake.set(handle as FfiEngineHandle, onWake);

  @override
  void ackWake(MockingbirdEngineHandle handle) =>
      _wake.ack(handle as FfiEngineHandle);

  @override
  int start(MockingbirdEngineHandle handle) =>
      bindings.mockingbird_start(_ptr(handle)).value;

  @override
  int stop(MockingbirdEngineHandle handle) =>
      bindings.mockingbird_stop(_ptr(handle)).value;

  @override
  int pushPcm(
    MockingbirdEngineHandle handle,
    Int16List samples,
    int captureTsUs,
  ) => _pcm.push(handle as FfiEngineHandle, samples, captureTsUs);

  @override
  MockingbirdPcmResult readPcm(
    MockingbirdEngineHandle handle,
    int maxSamples,
  ) => _pcm.read(handle as FfiEngineHandle, maxSamples);

  @override
  MockingbirdEventResult pollEvent(MockingbirdEngineHandle handle) =>
      _queues.pollEvent(handle as FfiEngineHandle);

  @override
  MockingbirdPacketResult drainPacket(MockingbirdEngineHandle handle) =>
      _queues.drainPacket(handle as FfiEngineHandle);

  @override
  MockingbirdStatsResult stats(MockingbirdEngineHandle handle) =>
      _queues.stats(handle as FfiEngineHandle);

  @override
  int updateConfig(
    MockingbirdEngineHandle handle,
    MockingbirdNativeConfig config,
  ) => _engines.updateConfig(handle as FfiEngineHandle, config);

  static ffi.Pointer<mockingbird_engine_t> _ptr(MockingbirdEngineHandle h) =>
      (h as FfiEngineHandle).pointer;
}
