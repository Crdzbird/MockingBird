## Unreleased

- The native platforms link a reduced ONNX Runtime 1.30.0 built for Mockingbird's models: statically into the engine on Android, Linux and Windows, and as a static xcframework with Swift Package Manager. The arm64 release APK of the example shrinks from 63.2 MB to 33.5 MB. Models are now `.ort` files everywhere, so keyword models must be converted to `.ort`; the bundled `.onnx` assets are gone. The Swift Package Manager floor on macOS drops from 14.0 to 12.0.
- Speaker verification: `engine.speakerEmbedding` makes a voice print from a few seconds of speech, `engine.setTargetSpeaker` compares every utterance with it, and `MockingbirdSpeakerEvent` says whether it matched. The CAM++ model downloads on first use through `MockingbirdModels.speakerModelPath()`.
- YAMNet is no longer bundled: `MockingbirdModels.soundClassPath()` downloads it on first use (5 MB), checks its SHA-256 and keeps it, so apps that never classify sounds ship 5 MB smaller. Downloads try each source three times, resume broken transfers, accept `onProgress`, and can use your own servers through `MockingbirdModels.mirrors`. `MockingbirdModels.soundClassAsset` is gone.
- Keyword spotting: `engine.addKeyword` listens for wake words and short commands with openWakeWord-compatible models and reports `MockingbirdKeywordEvent`. The shared front end ships as two assets, 1.6 MB together; `MockingbirdModels.keywordMelspectrogramPath()`, `keywordEmbeddingPath()` and `fromAsset()` return the paths. It runs only while there is sound and, with voice detection on, recent voice, and scores the same as if it ran throughout.
- With the neural tier, `MockingbirdSpeechBeginEvent.timestampUs` is where speech began rather than the frame that confirmed it, a window or two later.
- The neural tier works on the web: `MockingbirdModels` returns paths to `.ort` models stored in the module's memory. Cached native model files are replaced when the bundled model changes size.
- `MockingbirdConfig.voiceProcessing` turns on the platform's echo cancellation, gain control and noise suppression; `stats.capture.voiceProcessing` reports what applied.
- `MockingbirdDevices.input()` lists microphones and `engine.selectInputDevice` picks one.
- Interruptions and route changes arrive as `MockingbirdCaptureInterruptedEvent`, `MockingbirdCaptureResumedEvent` and `MockingbirdRouteChangedEvent`; on Android a disconnected stream now reopens on the new device instead of going silent.
- `MockingbirdBackend.auto` now means Core Audio on Apple and Oboe on Android, as documented, instead of miniaudio.
- An engine that captures from the microphone can be started again after `stop`; the second start used to fail with an invalid-state error.
- ONNX Runtime telemetry is switched off: its 1.30 builds would otherwise upload usage events and a device id to Microsoft. The engine sets `ORT_DISABLE_TELEMETRY=1` before its first ONNX Runtime environment, unless the app set it, and disables telemetry events on it.
- `engine.segments` delivers each finished segment with all its packets, and `MockingbirdSegment.toFile()` turns one into a WAV or Ogg Opus file.
- The bundled YAMNet model stores its weights as int8: 5.0 MB instead of 16.1 MB, with the same class on 98% of confident frames in our checks. The release APK for arm64 drops from 73.8 MB to 63.2 MB.
- On native platforms the engine wakes Dart as soon as it queues an event or packet, so events no longer wait for a 50 ms timer; `start(pollInterval:)` now paces only `stats`, every 250 ms by default. The web keeps its 50 ms timer.
- Linux and Windows support through `mockingbird_linux` and `mockingbird_windows`; the bundled models now ship to both.
- `MockingbirdCodec.oggOpus` and `MockingbirdEncoderConfig.oggOpus()`: a segment's packets, concatenated, form a playable `.opus` file.
- `MockingbirdWav.fromPcm16` wraps a PCM segment in a WAV header.
- The packet that closes a segment now has `isLast` set; it was never set before.
- `segments.maxMs` is enforced: a segment that reaches it closes and the next active frame opens a new one with fresh begin events.
- `segments.postRollMs` is honoured: a segment keeps the longer of the hangover and the post-roll after the detectors go quiet.
- Each segment's encoder state starts fresh, so every segment decodes on its own.
- A flickering voice inside one segment no longer repeats `MockingbirdSpeechBeginEvent` without an end in between.

## 0.1.0

First release.

- Microphone capture on Android (Oboe), iOS and macOS (Core Audio), or host-pushed PCM.
- Fast VAD (libfvad) and neural VAD (Silero v6), anomaly gate, YAMNet sound classification.
- Optional RNNoise denoising, Opus or PCM packets, segment pre-roll and hangover.
- One sensitivity dial, live retuning, silence gate, adaptive degradation, worker health stats.
- Events, packets and stats as Dart streams; no platform channels.
- Web support through `mockingbird_web`: the engine as WebAssembly with Web Audio capture.
- `Mockingbird.initialize()` to call once at startup.
