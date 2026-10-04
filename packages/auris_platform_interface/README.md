# auris_platform_interface

The platform interface for the [auris](https://pub.dev/packages/auris) plugin.
It holds the `dart:ffi` bindings generated from the engine's C header and the
`AurisPlatform` base class that each platform package registers.

App code does not depend on this package. Depend on `auris`, which pulls in the
platform packages for Android, iOS and macOS.

## What a platform package provides

`AurisPlatform` has one abstract member:

```dart
abstract class AurisPlatform extends PlatformInterface {
  ffi.DynamicLibrary get nativeLibrary;
  AurisBindings get bindings;
}
```

A platform package returns the library that contains the C ABI: the process
itself on Apple platforms, where the engine is linked statically into the app,
or `libauris.so` on Android. `bindings` wraps that library once and is shared
by every engine the app creates.

To add a platform, extend `AurisPlatform`, return the right `DynamicLibrary`,
and set `AurisPlatform.instance` in the plugin's `registerWith`.

## Regenerating the bindings

The bindings are generated from `include/auris/auris.h` in the engine
repository with `ffigen`:

```bash
dart run ffigen --config ffigen.yaml
```

Run it after any change to the header, then re-run the tests of `auris`.
