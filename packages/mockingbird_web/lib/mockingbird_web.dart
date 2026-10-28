/// The web implementation of the mockingbird plugin.
library;

import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_web_base.dart';
import 'package:mockingbird_web/src/module_registry.dart';
import 'package:mockingbird_web/src/web_assets.dart';
import 'package:mockingbird_web/src/web_devices.dart';
import 'package:mockingbird_web/src/web_engine_handle.dart';

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
}
