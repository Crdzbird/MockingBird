/// A mirror of `mockingbird_device_t`.
final class MockingbirdNativeDevice {
  /// Creates the record.
  const factory MockingbirdNativeDevice({
    required String id,
    required String name,
    required bool isDefault,
  }) = MockingbirdNativeDevice._;

  const MockingbirdNativeDevice._({
    required this.id,
    required this.name,
    required this.isDefault,
  });

  /// The opaque device id `mockingbird_set_input_device` takes.
  final String id;

  /// The name the system shows.
  final String name;

  /// Whether this is the system default input.
  final bool isDefault;
}
