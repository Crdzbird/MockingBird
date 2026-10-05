import 'package:mockingbird/src/mockingbird_status.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Facts about the linked engine.
final class Mockingbird {
  const Mockingbird._();

  /// Prepares the engine for use. Required on the web, where it loads the
  /// WebAssembly module; a no-op elsewhere, so apps can always call it.
  static Future<void> initialize() => MockingbirdPlatform.instance.initialize();

  /// The engine version, `X.Y.Z`.
  static String get version => MockingbirdPlatform.instance.version();

  /// The engine's own name for [status].
  static String statusName(MockingbirdStatus status) =>
      MockingbirdPlatform.instance.statusName(status.nativeValue);
}
