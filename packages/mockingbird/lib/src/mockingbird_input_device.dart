import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// A microphone the system offers.
final class MockingbirdInputDevice {
  /// Creates a device entry.
  const factory MockingbirdInputDevice({
    required String id,
    required String name,
    required bool isDefault,
  }) = MockingbirdInputDevice._;

  const MockingbirdInputDevice._({
    required this.id,
    required this.name,
    required this.isDefault,
  });

  /// Copies a native device record.
  factory MockingbirdInputDevice.fromNative(MockingbirdNativeDevice d) =>
      MockingbirdInputDevice(id: d.id, name: d.name, isDefault: d.isDefault);

  /// The id `MockingbirdEngine.selectInputDevice` takes.
  final String id;

  /// The name the system shows. Browsers leave it empty until the page has
  /// been granted the microphone.
  final String name;

  /// Whether this is the system's default input.
  final bool isDefault;

  @override
  String toString() => 'MockingbirdInputDevice($name, $id)';
}
