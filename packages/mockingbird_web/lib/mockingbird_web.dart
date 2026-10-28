/// The web implementation of the mockingbird plugin.
library;

import 'dart:typed_data';

import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_web_base.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_assets.dart';
import 'package:mockingbird_web/src/web_devices.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';
import 'package:mockingbird_web/src/web_keywords.dart';
import 'package:mockingbird_web/src/web_model_download.dart';
import 'package:mockingbird_web/src/web_model_store.dart';

/// Runs the engine as WebAssembly in the page and captures through Web Audio.
final class MockingbirdWeb extends MockingbirdWebBase {
  /// Creates the platform; [registerWith] installs it.
  factory MockingbirdWeb() = MockingbirdWeb._;

  MockingbirdWeb._();

  /// Where the wasm, loader and worklet are served from; null means the
  /// asset bundle. Set before `initialize`.
  static String? get assetBaseUrl => WebAssets.baseUrl;
  static set assetBaseUrl(String? url) => WebAssets.baseUrl = url;

  /// Called by the plugin registrant; also starts loading the module.
  static void registerWith(Registrar registrar) {
    MockingbirdPlatform.instance = MockingbirdWeb();
    ModuleRegistry.begin();
  }

  @override
  Future<MockingbirdDevicesResult> listInputDevices(int backend) =>
      const WebDevices().list();

  @override
  int setInputDevice(MockingbirdEngineHandle handle, String? id) =>
      const WebDevices().select(handle as WebEngineHandle, id);

  @override
  int setKeywordFrontend(
    MockingbirdEngineHandle handle,
    String? melspectrogram,
    String? embedding,
  ) => WebKeywords(
    ModuleRegistry.module,
  ).frontend(handle as WebEngineHandle, melspectrogram, embedding);

  @override
  MockingbirdKeywordResult addKeyword(
    MockingbirdEngineHandle handle,
    String path,
    double threshold,
  ) => WebKeywords(
    ModuleRegistry.module,
  ).add(handle as WebEngineHandle, path, threshold);

  @override
  int clearKeywords(MockingbirdEngineHandle handle) =>
      WebKeywords(ModuleRegistry.module).clear(handle as WebEngineHandle);

  @override
  Future<String> downloadModel(
    MockingbirdModelSpec spec, {
    required String directory,
    MockingbirdDownloadProgress? onProgress,
  }) async {
    final bytes = await const WebModelDownload().fetch(spec, onProgress);
    return await storeModel(spec.fileName, bytes);
  }

  @override
  Future<String> storeModel(String name, Uint8List bytes) async {
    await initialize();
    return WebModelStore(ModuleRegistry.module).store(name, bytes);
  }
}
