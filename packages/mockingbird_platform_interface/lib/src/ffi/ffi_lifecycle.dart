import 'dart:async';
import 'dart:collection';
import 'dart:ffi';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The done callback of one engine or player as a `NativeCallable.listener`:
/// calls queued on its native lifecycle thread complete their Futures here,
/// in the order the thread ran them, without blocking this isolate.
final class FfiLifecycle {
  /// Creates a lifecycle with no callback yet.
  factory FfiLifecycle() = FfiLifecycle._;

  FfiLifecycle._();

  final _pending = ListQueue<Completer<int>>();
  NativeCallable<mockingbird_done_cbFunction>? _done;

  /// Calls [submit] with the done callback and completes with the status the
  /// lifecycle thread reports, or with [submit]'s own status if it queued
  /// nothing.
  Future<int> run(int Function(mockingbird_done_cb done) submit) {
    final done = _done ??= NativeCallable<mockingbird_done_cbFunction>.listener(
      _report,
    );
    final completer = Completer<int>();
    _pending.add(completer);
    done.keepIsolateAlive = true;
    final status = submit(done.nativeFunction);
    if (status == MockingbirdStatusCode.ok) return completer.future;
    _pending.removeLast();
    _settle();
    return Future.value(status);
  }

  /// Frees the callback once the native object is destroyed, which runs
  /// every queued call first; a call whose report never arrived completes
  /// with `invalidState`.
  void close() {
    _done?.close();
    _done = null;
    while (_pending.isNotEmpty) {
      _pending.removeFirst().complete(MockingbirdStatusCode.invalidState);
    }
  }

  void _report(Pointer<Void> user, int op, int status) {
    if (_pending.isEmpty) return;
    _pending.removeFirst().complete(status);
    _settle();
  }

  void _settle() {
    if (_pending.isEmpty) _done?.keepIsolateAlive = false;
  }
}
