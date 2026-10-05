# mockingbird_ios

The iOS implementation of the [mockingbird](https://pub.dev/packages/mockingbird) plugin.
It is endorsed, so depending on `mockingbird` includes it automatically.

## What it bundles

- The Auris engine compiled from source into the host app: capture, RNNoise,
  libfvad, the anomaly gate, the Opus encoder, and the ONNX Runtime detectors
  (Silero VAD and YAMNet).
- Microphone capture through a Core Audio RemoteIO unit, which supplies the
  OS host-time stamp of every buffer. The plugin configures and activates the
  recording `AVAudioSession` itself.

The Dart side reaches the C ABI through `DynamicLibrary.process()`. There are
no platform channels.

## Integration

Both CocoaPods and Swift Package Manager are supported.

| Integration | Minimum iOS |
|---|---|
| CocoaPods | 15.0 |
| Swift Package Manager | 15.0 |

ONNX Runtime comes from the `onnxruntime-c` pod or from Microsoft's
`onnxruntime-swift-package-manager` package, depending on the integration.

## Requirements

Add a usage description to `Info.plist`, or the system denies the microphone
without a prompt:

```xml
<key>NSMicrophoneUsageDescription</key>
<string>Used to detect voice and sounds.</string>
```

If you request the permission with `permission_handler`, add
`PERMISSION_MICROPHONE=1` to the `post_install` block of your Podfile. Engines
created with `AurisBackend.hostPcm` do not touch the microphone.
