import 'package:auris/src/auris_status.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';

/// Thrown when a native call returns anything but [AurisStatus.ok].
final class AurisException implements Exception {
  /// Creates an exception for [call] that returned [status].
  const AurisException(this.call, this.status);

  /// The native function that failed.
  final String call;

  /// The status it returned.
  final AurisStatus status;

  /// Throws an [AurisException] unless [status] is OK.
  static void check(auris_status status, String call) {
    if (status != auris_status.AURIS_OK) {
      throw AurisException(call, AurisStatus.fromNative(status.value));
    }
  }

  @override
  String toString() => 'AurisException: $call returned ${status.name}';
}
