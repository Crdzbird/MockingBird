import 'dart:async';
import 'dart:collection';
import 'dart:ffi';
import 'dart:isolate';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The done reports of one engine or player as messages on a
/// [RawReceivePort]: calls queued on its native lifecycle thread complete
/// their Futures here, in the order the thread ran them, without blocking
/// this isolate. The thread posts and never calls Dart code, so an isolate
/// that dies with a call still queued leaves nothing freed behind.
final class FfiLifecycle {
  /// Creates a lifecycle with no port yet.
  factory FfiLifecycle() = FfiLifecycle._;

  FfiLifecycle._();

  final _pending = ListQueue<Completer<int>>();
  RawReceivePort? _port;

  /// Calls [submit] with the post function and the port and completes with
  /// the status the lifecycle thread reports, or with [submit]'s own status
  /// if it queued nothing.
  Future<int> run(int Function(mockingbird_post_cb post, int port) submit) {
    final port = _port ??= RawReceivePort(_report, 'mockingbird lifecycle');
    final completer = Completer<int>();
    _pending.add(completer);
    port.keepIsolateAlive = true;
    final status = submit(
      NativeApi.postCObject.cast(),
      port.sendPort.nativePort,
    );
    if (status == MockingbirdStatusCode.ok) return completer.future;
    _pending.removeLast();
    _settle();
    return Future.value(status);
  }

  /// Closes the port once the native object is destroyed, which runs every
  /// queued call first; a call whose report never arrived completes with
  /// `invalidState`.
  void close() {
    _port?.close();
    _port = null;
    while (_pending.isNotEmpty) {
      _pending.removeFirst().complete(MockingbirdStatusCode.invalidState);
    }
  }

  void _report(Object? message) {
    if (message is! int || _pending.isEmpty) return;
    _pending.removeFirst().complete(message.toUnsigned(32).toSigned(32));
    _settle();
  }

  void _settle() {
    if (_pending.isEmpty) _port?.keepIsolateAlive = false;
  }
}
