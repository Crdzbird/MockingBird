## Unreleased

- An engine that captures from the microphone can be started again after `stop`; the second start used to fail with an invalid-state error.
- ONNX Runtime telemetry is switched off: its 1.30 builds would otherwise upload usage events and a device id to Microsoft. The engine sets `ORT_DISABLE_TELEMETRY=1` before its first ONNX Runtime environment, unless the app set it, and disables telemetry events on it.
- Native engine with the Ogg Opus encoder and segment-end fixes.

## 0.1.0

First release.
