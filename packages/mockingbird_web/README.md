# mockingbird_web

The web implementation of the
[mockingbird](https://pub.dev/packages/mockingbird) plugin. It is endorsed, so
depending on `mockingbird` includes it automatically.

## What it bundles

- The Mockingbird engine compiled to WebAssembly with Emscripten: capture
  ingest, RNNoise, libfvad, the anomaly gate, the Opus encoders, and Silero VAD
  and YAMNet through a minimal ONNX Runtime linked into the module. The engine
  runs single-threaded inside the page; each pushed chunk is processed on the
  spot, so no cross-origin isolation headers are needed.
- Microphone capture through Web Audio: `getUserMedia`, an `AudioContext` opened
  at the engine's sample rate, and an `AudioWorklet` that delivers 20 ms int16
  chunks, resampling if the browser would not open at that rate.
- Two builds of the engine, about 9 MB each with their glue, and the capture
  worklet. `mockingbird_simd.js` and `mockingbird_simd.wasm` use WebAssembly
  SIMD and need Chrome 91, Firefox 89 or Safari 16.4 or later;
  `mockingbird.js` and `mockingbird.wasm` run everywhere else. The package
  checks once, with `WebAssembly.validate`, and fetches only the build the
  browser can run, so a visitor downloads one of them. SIMD makes noise
  suppression and the networks about twice as fast. Both builds produce the
  same packets and events.

The Dart side reaches the engine through `dart:js_interop`; the `mockingbird`
API is the same as on the other platforms.

## What differs on the web

| Feature | Web |
|---|---|
| Fast VAD, anomaly gate, RNNoise, Opus and PCM packets, segments, retuning, stats | available |
| Neural VAD (Silero) and YAMNet classification | available; the models load from the `.ort` assets, fetched only when asked for |
| Keyword spotting | available; the front end loads from `.ort` assets, and keyword models must be `.ort` too |
| `MockingbirdModels` | stores the `.ort` models in the module's memory and returns their paths |
| `MockingbirdBackend.auto` | capture through Web Audio; `hostPcm` works as everywhere |

Call `await Mockingbird.initialize()` before creating an engine. On the web it
loads the module; on the other platforms it returns at once, so apps can call it
unconditionally at startup.

```dart
Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Mockingbird.initialize();
  runApp(const MyApp());
}
```

The browser asks for the microphone on the first `start()` of a capture engine.
A refusal arrives as a `MockingbirdBackendErrorEvent` with status `permission`
on `engine.events`.

## Hosting the files elsewhere

Set `MockingbirdWeb.assetBaseUrl` to a directory URL before
`Mockingbird.initialize()` to serve the four files from a CDN instead of the
asset bundle. The server must return `application/wasm` for the two `.wasm`
files and a JavaScript type for the two scripts.

## Requirements

- A secure context (`https://` or `localhost`) for `getUserMedia`.
- A browser with `AudioWorklet` and ES modules: every current Chrome, Edge,
  Firefox and Safari.
