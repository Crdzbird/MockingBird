import 'package:auris/src/auris_status.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';

/// A native call returned a status other than [AurisStatus.ok].
final class AurisException implements Exception {
  /// Creates an exception for [call] that returned [status].
  const AurisException(this.call, this.status);

  /// The C ABI function that failed.
  final String call;

  /// The status it returned.
  final AurisStatus status;

  /// Throws unless [status] is `AURIS_OK`.
  static void check(int status, String call) {
    if (status != AurisStatusCode.ok) {
      throw AurisException(call, AurisStatus.fromNative(status));
    }
  }

  @override
  String toString() => 'AurisException: $call returned ${status.name}';
}
