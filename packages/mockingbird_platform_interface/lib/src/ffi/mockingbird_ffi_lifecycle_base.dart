import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engines.dart';
import 'package:mockingbird_platform_interface/src/ffi/mockingbird_ffi_platform_base.dart';

/// Engine start, stop and close on the engine's own lifecycle thread, so
/// opening and closing the microphone never blocks the calling isolate.
abstract class MockingbirdFfiLifecycleBase extends MockingbirdFfiPlatformBase {
  late final FfiEngines _lifecycle = FfiEngines(bindings);

  @override
  Future<int> startAsync(MockingbirdEngineHandle handle) =>
      _run(handle, .MOCKINGBIRD_ENGINE_OP_START);

  @override
  Future<int> stopAsync(MockingbirdEngineHandle handle) =>
      _run(handle, .MOCKINGBIRD_ENGINE_OP_STOP);

  @override
  Future<void> destroyAsync(MockingbirdEngineHandle handle) async {
    await _run(handle, .MOCKINGBIRD_ENGINE_OP_CLOSE);
    destroy(handle);
  }

  Future<int> _run(MockingbirdEngineHandle handle, mockingbird_engine_op op) =>
      _lifecycle.run(handle as FfiEngineHandle, op);
}
