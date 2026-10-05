/// The web implementation of the auris plugin.
library;

import 'dart:async';
import 'dart:js_interop';
import 'dart:typed_data';

import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_assets.dart';
import 'package:mockingbird_web/src/web_engine.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_lifecycle.dart';

/// Runs the engine as WebAssembly in the page and captures through Web Audio.
class AurisWeb extends AurisPlatform {
  /// Creates the platform; [registerWith] installs it and starts loading.
  AurisWeb();

  late final WebLifecycle _lifecycle = WebLifecycle(() => _ready, pushPcm);

  /// Where the wasm, its loader and the worklet are served from; null uses
  /// the asset bundle. Set before [initialize] to host them elsewhere.
  static String? get assetBaseUrl => WebAssets.baseUrl;
  static set assetBaseUrl(String? url) => WebAssets.baseUrl = url;

  /// Called by the plugin registrant; also starts loading the module.
  static void registerWith(Registrar registrar) {
    AurisPlatform.instance = AurisWeb();
    ModuleRegistry.begin();
  }

  @override
  Future<void> initialize() => ModuleRegistry.ready();

  WebEngine get _engine => WebEngine(_ready);

  AurisModule get _ready => ModuleRegistry.module;

  @override
  String version() => _ready.utf8ToString(_ready.versionString()).toDart;

  @override
  String statusName(int status) =>
      _ready.utf8ToString(_ready.statusStr(status)).toDart;

  @override
  AurisCreateResult create(AurisNativeConfig config) => _engine.create(config);

  @override
  void destroy(AurisEngineHandle handle) {
    final h = handle as WebEngineHandle;
    h.capture?.stop();
    _engine.destroy(h);
  }

  @override
  int start(AurisEngineHandle handle) =>
      _lifecycle.start(handle as WebEngineHandle);

  @override
  int stop(AurisEngineHandle handle) =>
      _lifecycle.stop(handle as WebEngineHandle);

  @override
  int pushPcm(AurisEngineHandle handle, Int16List samples, int captureTsUs) =>
      _engine.pushPcm(handle as WebEngineHandle, samples, captureTsUs);

  @override
  AurisPcmResult readPcm(AurisEngineHandle handle, int maxSamples) =>
      _engine.readPcm(handle as WebEngineHandle, maxSamples);

  @override
  AurisEventResult pollEvent(AurisEngineHandle handle) {
    if (_lifecycle.takePending() case final event?) {
      return (status: AurisStatusCode.ok, event: event);
    }
    return _engine.pollEvent(handle as WebEngineHandle);
  }

  @override
  AurisPacketResult drainPacket(AurisEngineHandle handle) =>
      _engine.drainPacket(handle as WebEngineHandle);

  @override
  AurisStatsResult stats(AurisEngineHandle handle) =>
      _engine.stats(handle as WebEngineHandle);

  @override
  int updateConfig(AurisEngineHandle handle, AurisNativeConfig config) =>
      _engine.updateConfig(handle as WebEngineHandle, config);
}
