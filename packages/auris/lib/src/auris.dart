import 'package:auris/src/auris_status.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';

/// Facts about the linked engine.
final class Auris {
  const Auris._();

  /// Prepares the engine for use. Required on the web, where it loads the
  /// WebAssembly module; a no-op elsewhere, so apps can always call it.
  static Future<void> initialize() => AurisPlatform.instance.initialize();

  /// The engine version, `X.Y.Z`.
  static String get version => AurisPlatform.instance.version();

  /// The engine's own name for [status].
  static String statusName(AurisStatus status) =>
      AurisPlatform.instance.statusName(status.nativeValue);
}
