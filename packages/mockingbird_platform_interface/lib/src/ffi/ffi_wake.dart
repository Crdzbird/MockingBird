import 'dart:ffi';
import 'dart:isolate';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The engine's wake as messages on a [RawReceivePort], one per engine. The
/// engine posts through `NativeApi.postCObject` and never calls Dart code,
/// so an isolate that shuts down without unregistering (hot restart, a
/// destroyed Flutter engine) leaves only a closed port, which the post
/// ignores.
final class FfiWake {
  /// Wraps [_bindings].
  const factory FfiWake(MockingbirdBindings bindings) = FfiWake._;

  const FfiWake._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Routes the engine's wake to [onWake] on this isolate, or unregisters
  /// and closes the port when [onWake] is null. The engine has let go of the
  /// previous port before it closes. True when registered.
  bool set(FfiEngineHandle h, void Function()? onWake) {
    final previous = h.wake;
    h.wake = null;
    if (onWake == null) {
      _bindings.mockingbird_set_wake_port(h.pointer, nullptr, 0);
      previous?.close();
      return true;
    }
    final port = RawReceivePort((Object? _) => onWake(), 'mockingbird wake');
    final s = _bindings
        .mockingbird_set_wake_port(
          h.pointer,
          NativeApi.postCObject.cast(),
          port.sendPort.nativePort,
        )
        .value;
    previous?.close();
    if (s != MockingbirdStatusCode.ok) {
      port.close();
      return false;
    }
    h.wake = port;
    return true;
  }

  /// Re-arms the wake; called before each drain.
  void ack(FfiEngineHandle h) => _bindings.mockingbird_ack_wake(h.pointer);
}
