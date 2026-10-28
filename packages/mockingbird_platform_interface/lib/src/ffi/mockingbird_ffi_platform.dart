import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_devices.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_keywords.dart';
import 'package:mockingbird_platform_interface/src/ffi/mockingbird_ffi_platform_base.dart';

/// The `dart:ffi` implementation; a platform package supplies the library.
abstract class MockingbirdFfiPlatform extends MockingbirdFfiPlatformBase {
  late final FfiDevices _devices = FfiDevices(bindings);
  late final FfiKeywords _keywords = FfiKeywords(bindings);

  @override
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) => wake.set(handle as FfiEngineHandle, onWake);

  @override
  void ackWake(MockingbirdEngineHandle handle) =>
      wake.ack(handle as FfiEngineHandle);

  @override
  Future<MockingbirdDevicesResult> listInputDevices(int backend) async =>
      _devices.list(backend);

  @override
  int setInputDevice(MockingbirdEngineHandle handle, String? id) =>
      _devices.select(handle as FfiEngineHandle, id);

  @override
  int setKeywordFrontend(
    MockingbirdEngineHandle handle,
    String? melspectrogram,
    String? embedding,
  ) => _keywords.frontend(handle as FfiEngineHandle, melspectrogram, embedding);

  @override
  MockingbirdKeywordResult addKeyword(
    MockingbirdEngineHandle handle,
    String path,
    double threshold,
  ) => _keywords.add(handle as FfiEngineHandle, path, threshold);

  @override
  int clearKeywords(MockingbirdEngineHandle handle) =>
      _keywords.clear(handle as FfiEngineHandle);
}
