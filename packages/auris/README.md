# Auris for Flutter

Flutter/Dart bindings for the [Auris](../../README.md) native audio capture +
detection engine. Capture → optional RNNoise → multilingual voice + anomaly +
YAMNet sound classification → encoded packets, delivered as Dart streams over
`dart:ffi`.

## Quick start

```dart
import 'package:auris/auris.dart';

final engine = AurisEngine.create(const AurisConfig())..start();

engine.events.listen((event) {
  switch (event) {
    case AurisSpeechBeginEvent():
      print('voice detected');
    case AurisSoundClassEvent(:final category):
      print('sound: $category');
    default:
      break;
  }
});

// ... later ...
await engine.dispose();
```

The default `AurisConfig` captures from the platform's microphone with fast VAD
and the anomaly gate. Everything else is grouped by concern:

| Group | Knobs |
|---|---|
| `vad` | `AurisVadConfig` — tier (`fast` / `neural`), thresholds, hangover, model path |
| `anomaly` | `AurisAnomalyConfig` — z-score triggers, background window |
| `soundClass` | `AurisSoundClassConfig` — YAMNet threshold, cadence, model path |
| `segments` | `AurisSegmentConfig` — pre-roll, post-roll, max length |
| `encoder` | `AurisEncoderConfig` — `pcm16` or `opus` (`.opus()` preset) |
| `queues` | `AurisQueueConfig` — ring sizes and the ungated PCM tap |
| `performance` | `AurisPerformanceConfig` — silence gate, adaptive degradation, wake batching (`.mobile()` preset) |

### One sensitivity dial

`AurisConfig.sensitive` maps one `0..1` value onto every detector and defaults to
multilingual neural (Silero) VAD. The bundled models are materialised on disk by
`AurisModels`, idempotently:

```dart
final config = AurisConfig.sensitive(
  0.6,
  neuralVadModelPath: await AurisModels.neuralVadPath(),
  soundClassModelPath: await AurisModels.soundClassPath(),
  denoise: true,
  performance: const AurisPerformanceConfig.mobile(),
);
```

## Streams

| Stream | Yields |
|---|---|
| `engine.events` | `AurisEvent` — a sealed hierarchy: `AurisSpeechBeginEvent`, `AurisSpeechEndEvent`, `AurisAnomalyEvent`, `AurisSegmentBeginEvent`, `AurisSegmentEndEvent`, `AurisSoundClassEvent`, `AurisOverrunEvent`, `AurisBackendErrorEvent`, `AurisDegradationEvent` |
| `engine.packets` | `AurisPacket` — encoded audio; `data` is a Dart-owned `Uint8List` copy, with `isPreroll` / `isPostroll` / `isKeyframe` / `isLast` |
| `engine.stats` | `AurisStats` — counters, input level, and `worker` health |

Because `AurisEvent` is sealed, a `switch` over it is exhaustive: the compiler
tells you when a new event type arrives. Each subtype carries a typed payload —
`AurisSoundClassEvent.category`, `AurisAnomalyEvent.reasons`,
`AurisDegradationEvent.level` — so there is no reason mask to decode.

```dart
engine.packets.listen((p) => socket.add(p.data)); // safe to keep or forward
```

Events are **polled** off the engine's own queues on a timer (50 ms by default),
not delivered through a C→Dart callback: the queue holds engine-owned copies,
the only safe path for a garbage-collected async runtime. A native error inside
a poll surfaces as an `AurisException` on `engine.events`.

## Live retuning

```dart
engine.updateConfig(AurisConfig.sensitive(0.8, /* same structure */));
```

Detector state survives, so an utterance in progress is not cut. Structural
fields (backend, rate, codec, queue sizes) must match the creating config.

## Feeding your own audio

```dart
final engine = AurisEngine.create(const AurisConfig(backend: .hostPcm))..start();
engine.pushPcm(samples, captureTsUs: ts); // Int16List, mono, at sampleRateHz
```

## Interpreting sound classes

```dart
final labels = await AurisModels.yamnetLabels(); // 521 AudioSet names
engine.events.listen((e) {
  if (e case AurisSoundClassEvent(:final classId, :final isHumanVoice)) {
    print('${labels[classId]} voice=$isHumanVoice'); // language-agnostic
  }
});
```

`AurisSoundCategory.of(classId)` folds the 521 classes into speech / humanOther /
animal / music / nature / other; AudioSet is English-biased, and non-English
speech often lands on "Conversation" or "Narration" — both map to `speech`.

## Example app

[`example/`](example) is a complete demo on `flutter_bloc`: a `DemoCubit` owns an
`EngineSession`, presets switch detector recipes, a slider retunes live, and every
widget lives in its own file.
