## 0.1.0

First release.

- Microphone capture on Android (Oboe), iOS and macOS (Core Audio), or host-pushed PCM.
- Fast VAD (libfvad) and neural VAD (Silero v6), anomaly gate, YAMNet sound classification.
- Optional RNNoise denoising, Opus or PCM packets, segment pre-roll and hangover.
- One sensitivity dial, live retuning, silence gate, adaptive degradation, worker health stats.
- Events, packets and stats as Dart streams; no platform channels.
- Web support through `auris_web`: the engine as WebAssembly with Web Audio capture.
- `Auris.initialize()` to call once at startup.
