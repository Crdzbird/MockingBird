/// The status codes of the C ABI as plain integers.
abstract final class MockingbirdStatusCode {
  /// Success.
  static const int ok = 0;

  /// `MOCKINGBIRD_ERR_INVALID_STATE`: not allowed in the engine's state.
  static const int invalidState = -2;

  /// `MOCKINGBIRD_ERR_QUEUE_EMPTY`: nothing to poll or drain right now.
  static const int queueEmpty = -7;

  /// `MOCKINGBIRD_ERR_UNSUPPORTED`: the feature is not in this build.
  static const int unsupported = -9;
}
