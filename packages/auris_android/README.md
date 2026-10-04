# auris_android

The Android implementation of the [auris](https://pub.dev/packages/auris)
plugin. It is endorsed, so depending on `auris` includes it automatically.

## What it bundles

- The Auris engine compiled from source by the plugin's CMake build: capture,
  RNNoise, libfvad, the anomaly gate, the Opus encoder, and the ONNX Runtime
  detectors (Silero VAD and YAMNet) through the `onnxruntime-android` AAR.
- Microphone capture through Oboe, with miniaudio as the fallback backend.
- 16 KB page alignment for the shared library, as Android 15 requires.

The Dart side opens `libauris.so` with `DynamicLibrary.open` and talks to it
over `dart:ffi`. There are no platform channels.

## Requirements

| Setting | Value |
|---|---|
| minSdk | 21 |
| compileSdk | 36 |
| NDK | r28 or newer |
| ABIs | arm64-v8a, x86_64 |

Add the microphone permission to your app's manifest and request it at runtime
before starting an engine that captures:

```xml
<uses-permission android:name="android.permission.RECORD_AUDIO" />
```

Engines created with `AurisBackend.hostPcm` do not touch the microphone and
need no permission.
