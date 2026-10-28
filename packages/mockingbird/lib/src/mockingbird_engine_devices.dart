part of 'mockingbird_engine.dart';

/// Choosing the microphone of a [MockingbirdEngine].
extension MockingbirdEngineDevices on MockingbirdEngine {
  /// Opens [id], from `MockingbirdDevices.input`, on the next [start]; null
  /// follows the system default. Only while stopped.
  void selectInputDevice(String? id) {
    _ensureLive();
    MockingbirdException.check(
      _platform.setInputDevice(_handle, id),
      'mockingbird_set_input_device',
    );
  }
}
