import 'package:mockingbird/src/mockingbird_backend.dart';
import 'package:mockingbird/src/mockingbird_exception.dart';
import 'package:mockingbird/src/mockingbird_input_device.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// The microphones an engine can open.
abstract final class MockingbirdDevices {
  /// Lists the inputs [backend] can open, the platform default by default.
  /// Android has no native listing: there, ask `AudioManager.getDevices`
  /// and pass `AudioDeviceInfo.getId()` as the id; this throws a
  /// [MockingbirdException] with `unsupported`.
  static Future<List<MockingbirdInputDevice>> input({
    MockingbirdBackend? backend,
  }) async {
    final which = backend ?? MockingbirdBackend.platformDefault;
    final r = await MockingbirdPlatform.instance.listInputDevices(
      which.nativeValue,
    );
    MockingbirdException.check(r.status, 'mockingbird_list_input_devices');
    return [for (final d in r.devices) MockingbirdInputDevice.fromNative(d)];
  }
}
