import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/module_loader.dart';

/// Holds the one WebAssembly module instance for the page.
abstract final class ModuleRegistry {
  static Future<MockingbirdModule>? _loading;
  static Future<void>? _settled;
  static MockingbirdModule? _module;

  /// Starts loading if nothing has yet; errors surface from [ready].
  static void begin() {
    if (_loading != null) return;
    final loading = const ModuleLoader().load();
    _loading = loading;
    _settled = loading.then<void>((m) => _module = m, onError: (Object _) {});
  }

  /// Completes when the module is loaded.
  static Future<void> ready() async {
    begin();
    await _settled;
    _module = await _loading;
  }

  /// The loaded module.
  static MockingbirdModule get module {
    final m = _module;
    if (m == null) {
      throw StateError(
        'mockingbird_web: await MockingbirdWeb.initialize() before creating '
        'an engine.',
      );
    }
    return m;
  }
}
