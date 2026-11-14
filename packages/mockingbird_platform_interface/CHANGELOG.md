## 0.2.1

- Version 0.2.1, with the engine that refuses Opus with 30 ms frames; no API change.

## 0.2.0

- Player and Opus decoder operations (`createPlayer`, `playerPush`, `playerStats`, `createDecoder`, `decoderDecode` and the rest), playback focus hooks, and their `dart:ffi` implementation; bindings regenerated for the new C entry points.
- `setSpeakerModel`, `speakerEmbedding` and `setTargetSpeaker`, with `MockingbirdSpeakerEmbeddingResult`.
- `downloadModel`, `MockingbirdModelSpec`, `MockingbirdModelCatalog` and `MockingbirdDownloadException`: verified, resumable model downloads with mirrors.
- `setKeywordFrontend`, `addKeyword` and `clearKeywords`, with `MockingbirdKeywordResult`.
- `storeModel`, for platforms without a file system.
- `voiceProcessing` in the native config, capture fields in the native stats (worker fields moved to `MockingbirdNativeWorkerStats`), and `listInputDevices` and `setInputDevice`.
- `setWakeListener` and `ackWake`, implemented over `mockingbird_set_wake_callback` and `mockingbird_ack_wake` with a `NativeCallable.listener`.
- FFI bindings regenerated for `MOCKINGBIRD_CODEC_OGG_OPUS`.

## 0.1.0

First release.

- `MockingbirdPlatform` with plain data records, usable on every platform.
- `ffi.dart`: the shared `dart:ffi` implementation for native platforms.
