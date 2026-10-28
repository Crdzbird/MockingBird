/// The web implementation of the mockingbird plugin.
library;

import 'dart:typed_data';

import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_assets.dart';
import 'package:mockingbird_web/src/web_engine.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_lifecycle.dart';

/// Runs the engine as WebAssembly in the page and captures through Web Audio.
final class MockingbirdWeb extends MockingbirdPlatform {
  /// Creates the platform; [registerWith] installs it.
  factory MockingbirdWeb() = MockingbirdWeb._;

  MockingbirdWeb._();

  late final WebLifecycle _lifecycle = WebLifecycle(() => _ready, pushPcm);

  /// Where the wasm, loader and worklet are served from; null means the
  /// asset bundle. Set before [initialize].
  static String? get assetBaseUrl => WebAssets.baseUrl;
  static set assetBaseUrl(String? url) => WebAssets.baseUrl = url;

  /// Called by the plugin registrant; also starts loading the module.
  static void registerWith(Registrar registrar) {
    MockingbirdPlatform.instance = MockingbirdWeb();
    ModuleRegistry.begin();
  }

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
