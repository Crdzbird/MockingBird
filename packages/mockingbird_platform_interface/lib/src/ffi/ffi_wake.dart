import 'dart:ffi';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// The engine's wake callback as a `NativeCallable.listener`, one per engine.
final class FfiWake {
  /// Wraps [_bindings].
  const factory FfiWake(MockingbirdBindings bindings) = FfiWake._;

  const FfiWake._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Routes the engine's wake to [onWake] on this isolate, or unregisters
  /// and frees the listener when [onWake] is null. True when registered.
  bool set(FfiEngineHandle h, void Function()? onWake) {
    final previous = h.wake;
    h.wake = null;
    if (onWake == null) {
      _bindings.mockingbird_set_wake_callback(h.pointer, nullptr, nullptr);
      previous?.close();
      return true;
    }
    final callable = NativeCallable<mockingbird_wake_cbFunction>.listener(
      (Pointer<Void> _) => onWake(),
    );
    final s = _bindings
        .mockingbird_set_wake_callback(
          h.pointer,
          callable.nativeFunction,
          nullptr,
        )
        .value;
    previous?.close();
    if (s != MockingbirdStatusCode.ok) {
      callable.close();
      return false;
    }
    h.wake = callable;
    return true;
  }

  /// Re-arms the wake; called before each drain.
  void ack(FfiEngineHandle h) => _bindings.mockingbird_ack_wake(h.pointer);
}
