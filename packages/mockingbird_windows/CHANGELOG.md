## 0.2.3

- The native engine gains `capture_performance` in `mockingbird_config_t` (`MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT`, `LOW_LATENCY`, `POWER_SAVING`), taking a reserved slot so the struct keeps its size. miniaudio maps `DEFAULT` and `LOW_LATENCY` to its low-latency profile, as before, and `POWER_SAVING` to its conservative one.
- `mockingbird_stats_t` gains `capture_gaps` and `capture_gap_ms`: audio the platform never delivered, estimated from the capture clock (more than 200 ms behind it). The default PCM ring grows to 32768 samples, two seconds at 16 kHz.
- `mockingbird_set_wake_port`, `mockingbird_submit_port` and `mockingbird_player_submit_port` post a message through a host post function (Dart's `NativeApi.postCObject`) instead of calling host code, so a host runtime that shuts down without unregistering leaves nothing freed for the engine to call.

## 0.2.2

- The native engine gains `mockingbird_submit` and `mockingbird_player_submit`, which run start, stop, pause and resume on the engine's or the player's own lifecycle thread, in the order submitted, and report each result through a callback, so opening or closing the device no longer blocks the caller. `MOCKINGBIRD_PLAYER_OP_CLOSE` and `MOCKINGBIRD_ENGINE_OP_CLOSE` stop if needed and release the device on that thread, so a destroy that follows has nothing slow left to do.

## 0.2.1

- The native engine refuses Opus and Ogg Opus with 30 ms frames when it is created, with `MOCKINGBIRD_ERR_UNSUPPORTED`. Opus has no 30 ms frame, so such an engine used to run and emit no packets.

## 0.2.0

First release.

- The Mockingbird engine built from source with MSVC, with microphone capture through miniaudio (WASAPI).
- Silero VAD and YAMNet through ONNX Runtime 1.30.0, downloaded and checked against its published SHA-256 at build time.
- Playback through miniaudio on the default output.
- ONNX Runtime is a reduced static build linked into the engine, downloaded and checked against its SHA-256 at build time, instead of Microsoft's full library.
- Native engine with speaker verification, keyword spotting, and SPEECH_BEGIN stamped where the neural tier found the onset.
- Native engine with platform voice processing, input device selection, and interruption and route-change handling.
- An engine that captures from the microphone can be started again after `stop`; the second start used to fail with an invalid-state error.
- ONNX Runtime telemetry is switched off: its 1.30 builds would otherwise upload usage events and a device id to Microsoft. The engine sets `ORT_DISABLE_TELEMETRY=1` before its first ONNX Runtime environment, unless the app set it, and disables telemetry events on it.
