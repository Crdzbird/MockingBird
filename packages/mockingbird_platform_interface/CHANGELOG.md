## Unreleased

- `voiceProcessing` in the native config, capture fields in the native stats (worker fields moved to `MockingbirdNativeWorkerStats`), and `listInputDevices` and `setInputDevice`.
- `setWakeListener` and `ackWake`, implemented over `mockingbird_set_wake_callback` and `mockingbird_ack_wake` with a `NativeCallable.listener`.
- FFI bindings regenerated for `MOCKINGBIRD_CODEC_OGG_OPUS`.

## 0.1.0

First release.

- `MockingbirdPlatform` with plain data records, usable on every platform.
- `ffi.dart`: the shared `dart:ffi` implementation for native platforms.
