/// The status codes of the C ABI as plain integers.
abstract final class AurisStatusCode {
  /// Success.
  static const int ok = 0;

  /// `AURIS_ERR_QUEUE_EMPTY`: nothing to poll or drain right now.
  static const int queueEmpty = -7;

  /// `AURIS_ERR_UNSUPPORTED`: the feature is not in this build.
  static const int unsupported = -9;
}
