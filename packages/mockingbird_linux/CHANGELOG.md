## Unreleased

- Native engine with speaker verification, keyword spotting, and SPEECH_BEGIN stamped where the neural tier found the onset.
- Native engine with platform voice processing, input device selection, and interruption and route-change handling.
- An engine that captures from the microphone can be started again after `stop`; the second start used to fail with an invalid-state error.
- ONNX Runtime telemetry is switched off: its 1.30 builds would otherwise upload usage events and a device id to Microsoft. The engine sets `ORT_DISABLE_TELEMETRY=1` before its first ONNX Runtime environment, unless the app set it, and disables telemetry events on it.
First release.

- The Mockingbird engine built from source, with microphone capture through miniaudio (PulseAudio, then ALSA).
- Silero VAD and YAMNet through ONNX Runtime 1.30.0, downloaded and checked against its published SHA-256 at build time.
