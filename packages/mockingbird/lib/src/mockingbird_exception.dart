import 'package:mockingbird/src/mockingbird_status.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// A native call returned a status other than [MockingbirdStatus.ok].
final class MockingbirdException implements Exception {
  /// Creates an exception for [call] that returned [status].
  const MockingbirdException(this.call, this.status);

  /// The C ABI function that failed.
  final String call;

  /// The status it returned.
  final MockingbirdStatus status;

  /// Throws unless [status] is `MOCKINGBIRD_OK`.
  static void check(int status, String call) {
    if (status != MockingbirdStatusCode.ok) {
      throw MockingbirdException(call, MockingbirdStatus.fromNative(status));
    }
  }

  @override
  String toString() => 'MockingbirdException: $call returned ${status.name}';
}
