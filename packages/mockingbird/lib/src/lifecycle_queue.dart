/// Runs lifecycle calls one after another, in the order they were made, so
/// a stop made while a start is still opening the device runs after it.
final class LifecycleQueue {
  /// Creates an empty queue.
  factory LifecycleQueue() = LifecycleQueue._;

  LifecycleQueue._();

  Future<void> _tail = Future<void>.value();

  /// Runs [call] once every call added before it has finished, and completes
  /// with its result or error. A failed call does not stop later ones.
  Future<T> add<T>(Future<T> Function() call) {
    final run = _tail.then((_) => call());
    _tail = run.then<void>((_) {}, onError: (Object _) {});
    return run;
  }
}
