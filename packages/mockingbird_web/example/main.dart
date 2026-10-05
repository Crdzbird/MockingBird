import 'package:mockingbird_web/mockingbird_web.dart';

/// Serves the WebAssembly files from a CDN instead of the asset bundle. Set
/// it before `Auris.initialize()`; apps use the `mockingbird` package.
void main() {
  AurisWeb.assetBaseUrl = 'https://cdn.example.com/mockingbird/';
}
