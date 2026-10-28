## Unreleased

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
