## 0.2.3

- `MockingbirdNativeOutputConfig.capturePerformance` and `MockingbirdNativeStats.captureGaps` and `captureGapMs`, optional so existing implementations keep compiling.
- The `dart:ffi` implementation registers the wake with `mockingbird_set_wake_port` and runs start, stop and close through `mockingbird_submit_port` and `mockingbird_player_submit_port`, all posting to a `RawReceivePort` through `NativeApi.postCObject`. It no longer hands the engine a `NativeCallable`, which the VM frees when the isolate shuts down even if it was never closed, so an engine outliving its isolate can no longer call freed code. Bindings regenerated.

## 0.2.2

- `MockingbirdPlatform.startAsync`, `stopAsync` and `destroyAsync`, which default to `start`, `stop` and `destroy`.
- The `dart:ffi` implementation runs `playerStart`, `playerPause`, `playerResume`, `playerStop`, `startAsync` and `stopAsync` on the native lifecycle thread through `mockingbird_player_submit` and `mockingbird_submit`, and completes each Future from a `NativeCallable.listener`, so none of them blocks the calling isolate any more. `destroyAsync` and `destroyPlayer` first release the device there with the new CLOSE operation. Bindings regenerated for the new entry points.

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
