# mockingbird_windows

The Windows implementation of the
[mockingbird](https://pub.dev/packages/mockingbird) plugin. It is endorsed, so
depending on `mockingbird` includes it automatically.

## What it bundles

- The Mockingbird engine compiled from source with MSVC into `mockingbird.dll`:
  capture, RNNoise, libfvad, the anomaly gate, the Opus and Ogg Opus encoders,
  and the ONNX Runtime detectors (Silero VAD and YAMNet).
- ONNX Runtime 1.30.0 for x64 or ARM64, downloaded from Microsoft's GitHub
  release the first time the app builds and checked against the SHA-256
  published for it. Only the headers, the import library and `onnxruntime.dll`
  are extracted. The build needs network access once per build tree.
- Microphone capture through miniaudio's WASAPI backend.

The DLLs sit next to the app's executable, which Windows searches before the
system directories, so the bundled ONNX Runtime wins over any copy that ships
with Windows. The Dart side reaches the C ABI through `DynamicLibrary.open`.
There are no platform channels.

## Requirements

- Visual Studio 2022 with the C++ desktop workload, which also provides CMake.
- Microphone access allowed for desktop apps under Settings, Privacy and
  security, Microphone. Engines created with `MockingbirdBackend.hostPcm` do
  not touch the microphone.
