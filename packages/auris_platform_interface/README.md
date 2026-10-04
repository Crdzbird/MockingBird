# auris_platform_interface

The platform interface for the [auris](https://pub.dev/packages/auris) plugin:
the `AurisPlatform` contract every platform package implements, the plain
data records that cross it, and a shared `dart:ffi` implementation for the
platforms that link the engine natively.

App code does not depend on this package. Depend on `auris`, which pulls in the
platform packages for Android, iOS, macOS and web.

## The contract

`AurisPlatform` exposes the engine's operations with plain Dart types and the
C ABI's status codes as integers; the `auris` package turns codes into
exceptions and records into its typed API:

```dart
abstract class AurisPlatform extends PlatformInterface {
  Future<void> initialize();
  String version();
  String statusName(int status);
  AurisCreateResult create(AurisNativeConfig config);
  void destroy(AurisEngineHandle handle);
  int start(AurisEngineHandle handle);
  int stop(AurisEngineHandle handle);
  int pushPcm(AurisEngineHandle handle, Int16List samples, int captureTsUs);
  AurisPcmResult readPcm(AurisEngineHandle handle, int maxSamples);
  AurisEventResult pollEvent(AurisEngineHandle handle);
  AurisPacketResult drainPacket(AurisEngineHandle handle);
  AurisStatsResult stats(AurisEngineHandle handle);
  int updateConfig(AurisEngineHandle handle, AurisNativeConfig config);
}
```

`AurisNativeConfig`, `AurisNativeEvent`, `AurisNativePacket` and
`AurisNativeStats` mirror the C structs field for field, with no `dart:ffi`
types, so the main library compiles on every platform including the web.

## The FFI implementation

`package:auris_platform_interface/ffi.dart` holds `AurisFfiPlatform`, a
complete implementation over the generated bindings. A platform package that
links the engine natively only supplies the library:

```dart
class AurisMacOS extends AurisFfiPlatform {
  static void registerWith() => AurisPlatform.instance = AurisMacOS();

  @override
  ffi.DynamicLibrary get nativeLibrary => ffi.DynamicLibrary.process();
}
```

The web package implements `AurisPlatform` directly over a WebAssembly
module. Import `ffi.dart` only from code that never runs on the web.

## Regenerating the bindings

The bindings are generated from `include/auris/auris.h` in the engine
repository with `ffigen`:

```bash
dart run ffigen --config ffigen.yaml
```

Run it after any change to the header, then re-run the tests of `auris`.
