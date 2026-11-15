## 0.2.2

- The native engine gains `mockingbird_submit` and `mockingbird_player_submit`, which run start, stop, pause and resume on the engine's or the player's own lifecycle thread, in the order submitted, and report each result through a callback, so opening or closing the device no longer blocks the caller. `MOCKINGBIRD_PLAYER_OP_CLOSE` and `MOCKINGBIRD_ENGINE_OP_CLOSE` stop if needed and release the device on that thread, so a destroy that follows has nothing slow left to do.

## 0.2.1

- The native engine refuses Opus and Ogg Opus with 30 ms frames when it is created, with `MOCKINGBIRD_ERR_UNSUPPORTED`. Opus has no 30 ms frame, so such an engine used to run and emit no packets.

## 0.2.0

- Playback through Oboe, and audio focus for the player: requested on start, abandoned on stop, with focus changes and the becoming-noisy broadcast passed to Dart over the plugin's first channel.
- ONNX Runtime is a reduced static build linked into the engine, downloaded and checked against its SHA-256 at build time, instead of Microsoft's full library.
- Native engine with speaker verification, keyword spotting, and SPEECH_BEGIN stamped where the neural tier found the onset.
- Native engine with platform voice processing, input device selection, and interruption and route-change handling.
- An engine that captures from the microphone can be started again after `stop`; the second start used to fail with an invalid-state error.
- ONNX Runtime telemetry is switched off: its 1.30 builds would otherwise upload usage events and a device id to Microsoft. The engine sets `ORT_DISABLE_TELEMETRY=1` before its first ONNX Runtime environment, unless the app set it, and disables telemetry events on it.
- Native engine with the Ogg Opus encoder and segment-end fixes.
- The Oboe capture and playback streams hold their callbacks by `shared_ptr`, so a stream that disconnects while the engine or a player is being destroyed no longer runs the error callback on freed memory.

## 0.1.0

First release.
