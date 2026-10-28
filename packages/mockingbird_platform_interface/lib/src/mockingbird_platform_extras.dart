import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/mockingbird_engine_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_native_device.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

/// The operations a platform may leave out; each default says it cannot.
abstract class MockingbirdPlatformExtras extends PlatformInterface {
  /// Passes the verification [token] up.
  MockingbirdPlatformExtras({required super.token});

  /// Calls [onWake] on this isolate soon after the engine queues an event or
  /// packet, or stops when [onWake] is null. False when this platform cannot
  /// push, in which case the caller polls on a timer.
  bool setWakeListener(
    MockingbirdEngineHandle handle,
    void Function()? onWake,
  ) => false;

  /// Re-arms the wake listener; call it before draining.
  void ackWake(MockingbirdEngineHandle handle) {}

  /// The capture devices of `mockingbird_backend_t` [backend].
  Future<MockingbirdDevicesResult> listInputDevices(int backend) async => (
    status: MockingbirdStatusCode.unsupported,
    devices: const <MockingbirdNativeDevice>[],
  );

  /// Chooses the device the next start opens; null follows the default.
  int setInputDevice(MockingbirdEngineHandle handle, String? id) =>
      MockingbirdStatusCode.unsupported;

  /// Stores model [bytes] where the engine can open them and returns the
  /// path; only platforms without a file system need it.
  Future<String> storeModel(String name, Uint8List bytes) =>
      throw UnsupportedError('storeModel is only needed on the web.');
}
