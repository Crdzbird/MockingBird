import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_engine.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_lifecycle.dart';

/// The engine operations over WebAssembly; `MockingbirdWeb` adds the rest.
abstract class MockingbirdWebBase extends MockingbirdPlatform {
  late final WebLifecycle _lifecycle = WebLifecycle(() => _ready, pushPcm);

  @override
  Future<void> initialize() => ModuleRegistry.ready();

  WebEngine get _engine => WebEngine(_ready);

  MockingbirdModule get _ready => ModuleRegistry.module;

  @override
  String version() => _engine.version();

  @override
  String statusName(int status) => _engine.statusName(status);

  @override
  MockingbirdCreateResult create(MockingbirdNativeConfig config) =>
      _engine.create(config);

  @override
  void destroy(MockingbirdEngineHandle handle) =>
      _engine.destroy(handle as WebEngineHandle);

  @override
  int start(MockingbirdEngineHandle handle) =>
      _lifecycle.start(handle as WebEngineHandle);

  @override
  int stop(MockingbirdEngineHandle handle) =>
      _lifecycle.stop(handle as WebEngineHandle);

  @override
  int pushPcm(
    MockingbirdEngineHandle handle,
    Int16List samples,
    int captureTsUs,
  ) => _engine.pushPcm(handle as WebEngineHandle, samples, captureTsUs);

  @override
  MockingbirdPcmResult readPcm(
    MockingbirdEngineHandle handle,
    int maxSamples,
  ) => _engine.readPcm(handle as WebEngineHandle, maxSamples);

  @override
  MockingbirdEventResult pollEvent(MockingbirdEngineHandle handle) {
    if (_lifecycle.takePending() case final event?) {
      return (status: MockingbirdStatusCode.ok, event: event);
    }
    return _engine.pollEvent(handle as WebEngineHandle);
  }

  @override
  MockingbirdPacketResult drainPacket(MockingbirdEngineHandle handle) =>
      _engine.drainPacket(handle as WebEngineHandle);

  @override
  MockingbirdStatsResult stats(MockingbirdEngineHandle handle) =>
      _engine.stats(handle as WebEngineHandle);

  @override
  int updateConfig(
    MockingbirdEngineHandle handle,
    MockingbirdNativeConfig config,
  ) => _engine.updateConfig(handle as WebEngineHandle, config);
}
