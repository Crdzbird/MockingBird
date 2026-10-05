import 'dart:ui_web' as ui_web;

/// Resolves the package's asset URLs.
///
/// By default the files come from the app's asset bundle, under
/// `packages/mockingbird_web/` inside an app and at the top level when this
/// package is the root, as in its own tests. [baseUrl] overrides both, for
/// apps that host `auris.js`, `auris.wasm` and the worklet elsewhere.
final class WebAssets {
  /// Creates the resolver.
  const WebAssets();

  /// A directory URL that holds the three files, or null for the bundle.
  static String? baseUrl;

  /// Absolute URLs to try, in order, for the asset called [name].
  List<String> candidates(String name) {
    if (baseUrl case final base?) {
      return [Uri.parse(base).resolve(name).toString()];
    }
    return [
      for (final key in [
        'packages/mockingbird_web/assets/$name',
        'assets/$name',
      ])
        Uri.base.resolve(ui_web.assetManager.getAssetUrl(key)).toString(),
    ];
  }
}
