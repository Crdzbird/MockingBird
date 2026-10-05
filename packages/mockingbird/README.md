# mockingbird

Native audio capture and detection for Flutter. Mockingbird listens to the
microphone (or to PCM you push), optionally denoises it, detects human voice in
any language and non-speech acoustic events, classifies what it hears, and
hands you only the audio that matters as encoded packets. Everything runs in
the Mockingbird C++ engine, reached through `dart:ffi`; there are no platform
channels on the hot path. The Dart API keeps the engine's name as its prefix:
`MockingbirdEngine`, `MockingbirdConfig`, `MockingbirdEvent`.

| Stage | What runs | Notes |
|---|---|---|
| Capture | Oboe on Android, Core Audio on iOS and macOS | int16 mono at 8, 16, 32 or 48 kHz |
| Denoise (optional) | RNNoise 0.2, full model | bridged to its native 48 kHz |
| Voice detection | libfvad (fast) or Silero v6 (neural) | language-agnostic |
| Anomaly gate | RMS, spectral flux and centroid z-scores | adaptive rolling background |
| Sound classification | YAMNet, 521 AudioSet classes | 3 s window, 1 s cadence |
| Encode | Opus or PCM | per segment, with pre-roll |

Supported platforms: Android, iOS, macOS and web. The web build runs the
engine as WebAssembly with fast VAD, the anomaly gate, RNNoise and Opus; the
neural detectors need ONNX Runtime and are not available there.

## Install

```bash
flutter pub add mockingbird
```

Then grant the microphone on each platform you ship to. The platform packages
document the exact keys:
[mockingbird_android](https://pub.dev/packages/mockingbird_android),
[mockingbird_ios](https://pub.dev/packages/mockingbird_ios),
[mockingbird_macos](https://pub.dev/packages/mockingbird_macos),
[mockingbird_web](https://pub.dev/packages/mockingbird_web). In short: the `RECORD_AUDIO`
permission on Android, `NSMicrophoneUsageDescription` on iOS and macOS, the
audio-input entitlement on macOS, and a secure context on the web. Engines
that only receive pushed PCM need none of it.

| Platform | Minimum |
|---|---|
| Android | API 21, compileSdk 37, NDK r28 or newer (16 KB pages) |
| iOS | 15.0 |
| macOS | 12.0 with CocoaPods, 14.0 with Swift Package Manager |
| Web | a browser with AudioWorklet and ES modules; no cross-origin isolation needed |

Call `Mockingbird.initialize()` once at startup. It loads the WebAssembly module on
the web and returns immediately elsewhere:

```dart
Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Mockingbird.initialize();
  runApp(const MyApp());
}
```

## Quick start

```dart
import 'package:mockingbird/mockingbird.dart';

final engine = MockingbirdEngine.create(const MockingbirdConfig())..start();

engine.events.listen((event) {
  switch (event) {
    case MockingbirdSpeechBeginEvent():
      print('voice');
    case MockingbirdSoundClassEvent(:final category, :final confidence):
      print('$category ${(confidence * 100).round()}%');
    default:
      break;
  }
});

// later
await engine.dispose();
```

The default config captures from the microphone with fast VAD and the anomaly
gate. To turn everything on with one dial:

```dart
final engine = MockingbirdEngine.create(
  MockingbirdConfig.sensitive(
    0.6,
    neuralVadModelPath: await MockingbirdModels.neuralVadPath(),
    soundClassModelPath: await MockingbirdModels.soundClassPath(),
    denoise: true,
    encoder: const MockingbirdEncoderConfig.opus(),
    performance: const MockingbirdPerformanceConfig.mobile(),
  ),
)..start();
```

`MockingbirdConfig.sensitive` maps one value in `0..1` onto every detector's
threshold and selects the neural VAD. `MockingbirdModels` copies the bundled Silero
and YAMNet models to an app-writable path once and returns their paths.

## Configuration

Everything is grouped by concern. Each group has sensible defaults, so you set
only what you change.

| Group | Type | Knobs |
|---|---|---|
| `vad` | `MockingbirdVadConfig` | tier (`fast` or `neural`), aggressiveness, neural threshold, hangover, model path |
| `anomaly` | `MockingbirdAnomalyConfig` | z-score triggers, background window |
| `soundClass` | `MockingbirdSoundClassConfig` | threshold, cadence, model path |
| `segments` | `MockingbirdSegmentConfig` | pre-roll, post-roll, maximum length |
| `encoder` | `MockingbirdEncoderConfig` | `pcm16` or `opus` (`MockingbirdEncoderConfig.opus()`), bitrate, VBR, complexity |
| `queues` | `MockingbirdQueueConfig` | ring sizes and the ungated PCM tap |
| `performance` | `MockingbirdPerformanceConfig` | silence gate, adaptive degradation, wake batching (`MockingbirdPerformanceConfig.mobile()`) |

## Streams

| Stream | Yields |
|---|---|
| `engine.events` | `MockingbirdEvent`, a sealed hierarchy: `MockingbirdSpeechBeginEvent`, `MockingbirdSpeechEndEvent`, `MockingbirdAnomalyEvent`, `MockingbirdSegmentBeginEvent`, `MockingbirdSegmentEndEvent`, `MockingbirdSoundClassEvent`, `MockingbirdOverrunEvent`, `MockingbirdBackendErrorEvent`, `MockingbirdDegradationEvent` |
| `engine.packets` | `MockingbirdPacket`: encoded audio in a `Uint8List` you own, with `isPreroll`, `isPostroll`, `isKeyframe`, `isLast` |
| `engine.stats` | `MockingbirdStats`: counters, input level in dBFS, queue fill, and `worker` health |

`MockingbirdEvent` is sealed, so a `switch` over it is exhaustive and each subtype
carries a typed payload: `MockingbirdSoundClassEvent.category`,
`MockingbirdAnomalyEvent.reasons`, `MockingbirdDegradationEvent.level`.

Events are polled off the engine's own queues on a timer (50 ms by default)
rather than delivered through a native callback, which keeps every object on
the Dart side engine-owned and copy-safe. A native error inside a poll surfaces
as a `MockingbirdException` on `engine.events`.

```dart
engine.packets.listen((p) => socket.add(p.data));
```

## Retune without restarting

```dart
engine.updateConfig(MockingbirdConfig.sensitive(0.8));
```

Thresholds change on the running engine; detector state survives, so an
utterance in progress is not cut. Structural fields (backend, sample rate,
codec, queue sizes) must match the config the engine was created with.

## Feed your own audio

```dart
final engine = MockingbirdEngine.create(
  const MockingbirdConfig(backend: .hostPcm),
)..start();
engine.pushPcm(samples, captureTsUs: ts); // Int16List, mono, at sampleRateHz
```

Files, network audio and test vectors go through the same pipeline as the
microphone. Output timestamps depend only on frame position, so the same input
always gives the same packets and events.

## Sound classes

```dart
final labels = await MockingbirdModels.yamnetLabels(); // 521 AudioSet names
engine.events.listen((e) {
  if (e case MockingbirdSoundClassEvent(:final classId, :final isHumanVoice)) {
    print('${labels[classId]} voice=$isHumanVoice');
  }
});
```

`MockingbirdSoundCategory.of(classId)` folds the 521 classes into speech,
humanOther, animal, music, nature and other. AudioSet is English-biased;
non-English speech often lands on "Conversation" or "Narration", and both map
to speech.

## How it compares

The pub.dev packages below each cover part of the same job. The first table is
about scope; the second is measured.

| | mockingbird | vad_plus 0.3.1 | vad 0.0.8 | sherpa_onnx 1.13.8 | flutter_nnnoiseless 1.4.0 | record 7.1.1 |
|---|---|---|---|---|---|---|
| Microphone capture | yes | yes | yes, through `record` | no | no | yes |
| Push your own PCM | yes | yes | yes | yes | yes | no |
| Voice detection | libfvad or Silero v6 | Silero v6 | Silero v4 or v5, model fetched from a CDN at runtime | Silero or TEN | RNNoise probability | no |
| Noise suppression | RNNoise | no | platform flags via the recorder | gtcrn, dpdfnet | RNNoise | platform flags |
| Sound classification | YAMNet, 521 classes | no | no | CED and zipformer audio tagging | no | no |
| Non-speech event gate | yes | no | no | no | no | no |
| Encoded output | Opus or PCM, per segment with pre-roll | PCM speech segments | PCM speech segments | PCM speech segments | PCM | AAC, Opus, WAV, FLAC, PCM |
| Retune while running | yes | no | no | no | no | no |
| Silence gate and adaptive load shedding | yes | no | no | no | no | no |
| Native path | `dart:ffi` | `dart:ffi` | `dart:ffi`, JS on web | `dart:ffi` | Rust bridge | platform channels |
| Platforms | Android, iOS, macOS, web | Android, iOS, macOS | Android, iOS, macOS, Windows, Linux, web | Android, iOS, macOS, Windows, Linux, web | Android, iOS, macOS, Windows, Linux, web | Android, iOS, macOS, Windows, Linux, web |
| License | MIT | MIT | MIT | Apache-2.0 | LGPL-3.0 | BSD-3-Clause |

sherpa_onnx also ships speech recognition, synthesis, keyword spotting and
speaker identification, which Mockingbird does not attempt.

### Measured

Each package ran in its own release-mode Flutter macOS app on an Apple M3 Max,
processing the same 80 s of 16 kHz audio (speech, music, an animal, silence)
pushed in the chunk size the package expects. The figure is process CPU time
divided by audio time: the share of one core needed to keep up with real time.
Memory is the peak resident size during the run. The harness, inputs and raw
results are in `bench/flutter_compare` of the repository.

| Task | Package and model | Share of one core | Peak RSS |
|---|---|---|---|
| Neural voice detection | mockingbird, Silero v6 | 0.67% | 145 MB |
| | sherpa_onnx, Silero v5 | 0.43% | 144 MB |
| | vad_plus, Silero v6 | 1.69% | 152 MB |
| | vad, Silero v5 | 2.07% | 166 MB |
| Noise suppression | mockingbird, RNNoise 0.2 full model | 1.12% | 141 MB |
| | flutter_nnnoiseless, RNNoise classic model | 0.60% | 112 MB |
| | sherpa_onnx, gtcrn | 4.26% | 154 MB |
| Sound classification, 1 s cadence | mockingbird, YAMNet (521 classes) | 1.80% | 164 MB |
| | sherpa_onnx, CED tiny fp32 | 1.28% | 174 MB |
| | sherpa_onnx, CED tiny int8 | 1.14% | 177 MB |
| Fast voice detection | mockingbird, libfvad | 0.14% | 114 MB |
| Anomaly gate | mockingbird | 0.07% | 145 MB |
| Everything on, with Opus | mockingbird | 3.19% | 193 MB |

The same engine as WebAssembly, run under V8 (the engine inside Chrome) on
the same input. The web build is single-threaded and has no SIMD, so the
neural network in RNNoise costs about twice what it does natively; the
packet and segment counts match the native run exactly.

| Task | mockingbird_web (WebAssembly) | Share of one core |
|---|---|---|
| Fast voice detection | libfvad | 0.02% |
| Anomaly gate | | 0.02% |
| Noise suppression | RNNoise 0.2 full model | 2.71% |
| Everything the web build has: fast VAD, anomaly gate, RNNoise, Opus | | 2.89% |

Measured on Flutter 3.47.6 with every package at its latest version; the
earlier run on 3.47.2 differed by at most a few hundredths of a percent, which
is the run-to-run noise of the harness.

What the Silero v6 upgrade changed is selectivity, not cost. On this fixture,
which repeats one utterance four times between music and an animal, the
neural tier now opens 12 speech segments where v5 opened 27 and ships 1,124
packets where it shipped 1,731, with the same four utterances detected. Less
false speech means less audio leaving the device.

Reading the numbers fairly:

- The mockingbird rows include the whole pipeline around the detector: segmentation
  with pre-roll, packetisation, the event queue and the Dart poll loop. The
  sherpa_onnx VAD row is the bare detector with no capture or packet path.
- The noise suppression rows run different RNNoise builds. Mockingbird compiles the
  0.2 full model and bridges 16 kHz to 48 kHz and back; flutter_nnnoiseless is
  a Rust port with the smaller classic model.
- CED tiny is a smaller network than YAMNet with its own label set; the rows
  compare the cost of classifying every second, not the quality of the labels.
- The `vad` package runs its frame loop in Dart on the main isolate, which is
  where its extra cost comes from.
- The web rows measure the push loop alone, because the WebAssembly build
  processes each chunk inline; the Flutter rows also include the Dart poll
  loop and the FFI copies. The native fast VAD and anomaly rows are at the
  noise floor of the harness either way.

Release bundle size on macOS, measured as the whole `.app` with the package's
default assets, against an empty Flutter app built the same way:

| Package | Bundle | Added over the empty app |
|---|---|---|
| empty Flutter app | 32.6 MB | |
| mockingbird | 126.3 MB | 93.8 MB |
| vad_plus | 89.4 MB | 56.8 MB |
| vad | 93.2 MB | 60.6 MB |
| sherpa_onnx | 88.3 MB | 55.7 MB |
| flutter_nnnoiseless | 46.3 MB | 13.7 MB |

The mockingbird bundle carries the Silero and YAMNet models as assets (17 MB) and a
universal binary. sherpa_onnx loads its models from disk, so its model files
(22 MB for CED tiny) are not in that figure, and `vad` downloads its model at
first run.

## Keeping it cheap on phones

- The silence gate (`silenceGateDbfs`, default -60) skips RNNoise and YAMNet
  while the input is quiet; VAD and the anomaly gate stay live. A quiet room
  with every feature on costs about 0.4% of a desktop core.
- `MockingbirdPerformanceConfig.mobile()` turns on adaptive degradation, which steps
  YAMNet's cadence down under load and back up when calm, and batches worker
  wake-ups to cut thread switches.
- `MockingbirdSoundClassConfig.intervalMs` is the biggest single lever: 1000 ms is
  the default, 500 ms roughly doubles the classifier's cost.
- `engine.stats` reports `worker.busyPpm`, wake-ups and the peak frame time,
  so you can see the cost on the device you ship to.

Expect roughly three times the desktop figures above on a recent iPhone and
five to eight times on a mid-range Android phone.

## Web

The web build is the same engine compiled to WebAssembly, running
single-threaded in the page and fed by an AudioWorklet. Fast VAD, the anomaly
gate, RNNoise, Opus and every API above work; the neural tier and YAMNet are
rejected with `unsupported` because ONNX Runtime is not part of the build, and
`MockingbirdModels` throws since there is no file system. The three assets add about
6 MB to the app. See [mockingbird_web](https://pub.dev/packages/mockingbird_web) for the
details and for hosting the files on a CDN.

## Example app

The [example](example) is a complete demo on `flutter_bloc`: presets switch
detector recipes, a slider retunes the running engine, an RNNoise toggle, a
level meter, the latest recognised sound and the full stats card. It runs on
macOS, iOS, Android and the web, where the presets that need the neural
detectors fall back to the fast tier.

## Engine

The engine is the Mockingbird C++ library, with a pure C ABI, bindings for Go and
Python, and a conformance harness that checks the three language surfaces
produce byte-identical output. Its repository has the build, the tests, the
benchmark and the design notes. Everything is MIT licensed; the vendored
dependencies are BSD, MIT or public domain.
