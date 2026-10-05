import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/module_loader.dart';

/// Holds the one WebAssembly module instance for the page.
abstract final class ModuleRegistry {
  static Future<AurisModule>? _loading;
  static AurisModule? _module;

  /// Starts loading if nothing has yet; errors surface from [ready].
  static void begin() {
    _loading ??= const ModuleLoader().load()
      ..then<void>((m) => _module = m, onError: (Object _) {});
  }

  /// Completes when the module is loaded.
  static Future<void> ready() async {
    begin();
    _module = await _loading!;
  }

  /// The loaded module.
  static AurisModule get module {
    final m = _module;
    if (m == null) {
      throw StateError(
        'mockingbird_web: await AurisWeb.initialize() before creating an engine.',
      );
    }
    return m;
  }
}
