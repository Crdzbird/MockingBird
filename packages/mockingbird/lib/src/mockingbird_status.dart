/// Result codes of the native engine.
enum MockingbirdStatus {
  /// Success.
  ok(0),

  /// A configuration or argument was rejected.
  invalidArg(-1),

  /// The call is not valid in the engine's current state.
  invalidState(-2),

  /// Allocation failed.
  noMemory(-3),

  /// The capture backend failed.
  backend(-4),

  /// The engine is not running.
  notRunning(-5),

  /// The engine is already running.
  alreadyRunning(-6),

  /// Nothing is queued.
  queueEmpty(-7),

  /// Microphone permission is missing.
  permission(-8),

  /// The feature was not compiled in or the input is unsupported.
  unsupported(-9),

  /// An internal error.
  internal(-10);

  const MockingbirdStatus(this.nativeValue);

  /// The `mockingbird_status_t` value.
  final int nativeValue;

  /// Decodes a native status; unknown values map to [internal].
  static MockingbirdStatus fromNative(int value) =>
      values.firstWhere((s) => s.nativeValue == value, orElse: () => internal);
}
