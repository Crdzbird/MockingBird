# mockingbird_macos

The macOS implementation of the [mockingbird](https://pub.dev/packages/mockingbird) plugin.
It is endorsed, so depending on `mockingbird` includes it automatically.

## What it bundles

- The Auris engine compiled from source into the host app: capture, RNNoise,
  libfvad, the anomaly gate, the Opus encoder, and the ONNX Runtime detectors
  (Silero VAD and YAMNet).
- Microphone capture through a Core Audio HALOutput unit bound to the default
  input device, with the OS host-time stamp of every buffer.

The Dart side reaches the C ABI through `DynamicLibrary.process()`. There are
no platform channels.

## Integration

Both CocoaPods and Swift Package Manager are supported.

| Integration | Minimum macOS |
|---|---|
| CocoaPods | 12.0 |
| Swift Package Manager | 14.0 |

The higher Swift Package Manager floor comes from Microsoft's
`onnxruntime-swift-package-manager` package. Set `MACOSX_DEPLOYMENT_TARGET`
in your Xcode project accordingly.

## Requirements

Add the audio-input entitlement to both entitlement files of your app and a
usage description to `Info.plist`:

```xml
<key>com.apple.security.device.audio-input</key>
<true/>
```

```xml
<key>NSMicrophoneUsageDescription</key>
<string>Used to detect voice and sounds.</string>
```

Request access with `AVCaptureDevice.requestAccess` or `permission_handler`
before starting an engine that captures. Engines created with
`AurisBackend.hostPcm` do not touch the microphone.
