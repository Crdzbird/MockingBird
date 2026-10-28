# mockingbird_platform_interface

The platform interface for the
[mockingbird](https://pub.dev/packages/mockingbird) plugin: the
`MockingbirdPlatform` contract every platform package implements, the plain data
records that cross it, and a shared `dart:ffi` implementation for the platforms
that link the engine natively.

App code does not depend on this package. Depend on `mockingbird`, which pulls
in the platform packages for Android, iOS, macOS and web.

## The contract

`MockingbirdPlatform` exposes the engine's operations with plain Dart types and
the C ABI's status codes as integers; the `mockingbird` package turns codes into
exceptions and records into its typed API:

```dart
abstract class MockingbirdPlatform extends PlatformInterface {
  Future<void> initialize();
  String version();
  String statusName(int status);
  MockingbirdCreateResult create(MockingbirdNativeConfig config);
  void destroy(MockingbirdEngineHandle handle);
  int start(MockingbirdEngineHandle handle);
  int stop(MockingbirdEngineHandle handle);
  int pushPcm(MockingbirdEngineHandle handle, Int16List samples, int captureTsUs);
  MockingbirdPcmResult readPcm(MockingbirdEngineHandle handle, int maxSamples);
  MockingbirdEventResult pollEvent(MockingbirdEngineHandle handle);
  MockingbirdPacketResult drainPacket(MockingbirdEngineHandle handle);
  MockingbirdStatsResult stats(MockingbirdEngineHandle handle);
  bool setWakeListener(MockingbirdEngineHandle handle, void Function()? onWake);
  void ackWake(MockingbirdEngineHandle handle);
  int updateConfig(MockingbirdEngineHandle handle, MockingbirdNativeConfig config);
}
```

`setWakeListener` returns false by default, which tells the caller to poll on a
timer; the FFI implementation registers a `NativeCallable.listener` with
`mockingbird_set_wake_callback` and returns true.

`MockingbirdNativeConfig`, `MockingbirdNativeEvent`, `MockingbirdNativePacket`
and `MockingbirdNativeStats` mirror the C structs field for field, with no
`dart:ffi` types, so the main library compiles on every platform including the
web.

## The FFI implementation

`package:mockingbird_platform_interface/ffi.dart` holds
`MockingbirdFfiPlatform`, a complete implementation over the generated bindings.
A platform package that links the engine natively only supplies the library:

```dart
class MockingbirdMacOS extends MockingbirdFfiPlatform {
  static void registerWith() => MockingbirdPlatform.instance = MockingbirdMacOS();

  @override
  ffi.DynamicLibrary get nativeLibrary => ffi.DynamicLibrary.process();
}
```

The web package implements `MockingbirdPlatform` directly over a WebAssembly
module. Import `ffi.dart` only from code that never runs on the web.

## Regenerating the bindings

The bindings are generated from `include/mockingbird/mockingbird.h` in the
engine repository, https://github.com/Crdzbird/MockingBird, with `ffigen`:

```bash
dart run ffigen --config ffigen.yaml
```

Run it after any change to the header, then re-run the tests of `mockingbird`.
