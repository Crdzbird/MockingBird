# mockingbird_web

The web implementation of the [mockingbird](https://pub.dev/packages/mockingbird)
plugin. It is endorsed, so depending on `mockingbird` includes it automatically.

## What it bundles

- The Auris engine compiled to WebAssembly with Emscripten: capture ingest,
  RNNoise, libfvad, the anomaly gate and the Opus encoder. The engine runs
  single-threaded inside the page; each pushed chunk is processed on the
  spot, so no cross-origin isolation headers are needed.
- Microphone capture through Web Audio: `getUserMedia`, an `AudioContext`
  opened at the engine's sample rate, and an `AudioWorklet` that delivers
  20 ms int16 chunks, resampling if the browser would not open at that rate.
- Three assets, about 6 MB in total: `auris.js`, `auris.wasm` and the
  capture worklet. They load lazily from the app's asset bundle.

The Dart side reaches the engine through `dart:js_interop`; the `mockingbird` API
is the same as on the other platforms.

## What differs on the web

| Feature | Web |
|---|---|
| Fast VAD, anomaly gate, RNNoise, Opus and PCM packets, segments, retuning, stats | available |
| Neural VAD (Silero) and YAMNet classification | not available; `AurisConfig` with those enabled is rejected with `unsupported` |
| `AurisModels` | throws `UnsupportedError`; there is no file system to extract to |
| `AurisBackend.auto` | capture through Web Audio; `hostPcm` works as everywhere |

Call `await Auris.initialize()` before creating an engine. On the web it
loads the module; on the other platforms it returns at once, so apps can
call it unconditionally at startup.

```dart
Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Auris.initialize();
  runApp(const MyApp());
}
```

The browser asks for the microphone on the first `start()` of a capture
engine. A refusal arrives as an `AurisBackendErrorEvent` with status
`permission` on `engine.events`.

## Hosting the files elsewhere

Set `AurisWeb.assetBaseUrl` to a directory URL before `Auris.initialize()`
to serve the three files from a CDN instead of the asset bundle. The server
must return `application/wasm` for `auris.wasm` and a JavaScript type for
the two scripts.

## Requirements

- A secure context (`https://` or `localhost`) for `getUserMedia`.
- A browser with `AudioWorklet` and ES modules: every current Chrome, Edge,
  Firefox and Safari.
