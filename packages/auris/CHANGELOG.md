## 0.1.0

First release.

- Microphone capture on Android (Oboe), iOS and macOS (Core Audio), or host-pushed PCM.
- Fast VAD (libfvad) and neural VAD (Silero v5), anomaly gate, YAMNet sound classification.
- Optional RNNoise denoising, Opus or PCM packets, segment pre-roll and hangover.
- One sensitivity dial, live retuning, silence gate, adaptive degradation, worker health stats.
- Events, packets and stats as Dart streams over `dart:ffi`; no platform channels.
