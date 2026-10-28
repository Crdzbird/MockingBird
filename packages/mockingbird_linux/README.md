# mockingbird_linux

The Linux implementation of the
[mockingbird](https://pub.dev/packages/mockingbird) plugin. It is endorsed, so
depending on `mockingbird` includes it automatically.

## What it bundles

- The Mockingbird engine compiled from source into `libmockingbird.so`:
  capture, RNNoise, libfvad, the anomaly gate, the Opus and Ogg Opus encoders,
  and the ONNX Runtime detectors (Silero VAD and YAMNet).
- ONNX Runtime 1.30.0 for x64 or arm64, downloaded from Microsoft's GitHub
  release the first time the app builds and checked against the SHA-256
  published for it. The build needs network access once per build tree.
- Microphone capture through miniaudio, which uses PulseAudio when it is
  running and ALSA otherwise. Both are loaded at run time, so the app has no
  link-time audio dependency.

The Dart side reaches the C ABI through `DynamicLibrary.open`. There are no
platform channels.

## Requirements

- CMake 3.24 or later, Clang 17 or GCC 13 or later, and the usual Flutter Linux
  toolchain.
- `libpulse` or `libasound` at run time for capture. Engines created with
  `MockingbirdBackend.hostPcm` do not touch the microphone.

Desktop Linux has no microphone permission prompt. Sandboxed packages, such as
Flatpak or Snap, need the audio or PulseAudio permission in their manifest.
