import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/web_assets.dart';

extension type _EsModule(JSObject _) implements JSObject {
  @JS('default')
  external MockingbirdModuleFactory get defaultExport;
}

/// Loads `mockingbird.js` from the package assets and instantiates the module.
final class ModuleLoader {
  /// Creates the loader.
  const ModuleLoader();

  /// Imports the script as an ES module and runs its factory.
  Future<MockingbirdModule> load() async {
    const assets = WebAssets();
    final scripts = assets.candidates('mockingbird.js');
    final wasms = assets.candidates('mockingbird.wasm');
    Object? failure;
    for (var i = 0; i < scripts.length; i++) {
      try {
        final module = await importModule(scripts[i].toJS).toDart as _EsModule;
        final wasm = wasms[i];
        final locate = ((JSString path) => wasm.toJS).toJS;
        final options = JSObject()..setProperty('locateFile'.toJS, locate);
        return await module.defaultExport.call(options).toDart;
      } on Object catch (e) {
        failure = e;
      }
    }
    throw StateError(
      'mockingbird_web: could not load mockingbird.js ($failure)',
    );
  }
}
