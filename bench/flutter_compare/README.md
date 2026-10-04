# Package comparison benchmark

Measures Auris against the pub.dev packages that cover parts of the same job,
each in its own Flutter macOS app so no two packages share a process or an
ONNX Runtime build.

| App | Package | Scenarios |
|---|---|---|
| `auris_bench` | `auris` (path dependency on this repository) | fast VAD, neural VAD, anomaly gate, RNNoise, YAMNet, everything on |
| `vad_plus_bench` | `vad_plus` | Silero VAD |
| `vad_bench` | `vad` | Silero VAD |
| `sherpa_bench` | `sherpa_onnx` | Silero VAD, gtcrn denoiser, CED tiny audio tagging |
| `nnnoiseless_bench` | `flutter_nnnoiseless` | RNNoise denoise |

## Method

- Input: the three fixtures in `tests/fixtures` (speech, music, a cat) joined
  with one second of silence after each and repeated four times, about 80 s of
  16 kHz mono audio. Every app builds the same buffer from the same files.
- Each scenario pushes that buffer through the package as a host would, in the
  chunk size the package expects, yielding to the event loop so callbacks and
  pollers run.
- Measured per scenario: process CPU time from `clock_gettime` with
  `CLOCK_PROCESS_CPUTIME_ID` (every thread the package uses), wall time,
  resident memory before and at peak, and the package's own counters (frames
  processed, segments, events) so the run can be checked for drops.
- `cpu_share` is CPU seconds divided by audio seconds: the fraction of one
  core the package needs to keep up with real time.
- Apps are built in release mode so Dart-side code runs ahead-of-time
  compiled, and the `.app` bundle size is recorded as the footprint a package
  adds on top of the Flutter template.

## Running

The sherpa-onnx models are not committed. Fetch them once:

```bash
cd bench/flutter_compare/models
curl -sL -o ced.tar.bz2 https://github.com/k2-fsa/sherpa-onnx/releases/download/audio-tagging-models/sherpa-onnx-ced-tiny-audio-tagging-2024-04-19.tar.bz2
tar xjf ced.tar.bz2 && rm ced.tar.bz2
curl -sL -o gtcrn_simple.onnx https://github.com/k2-fsa/sherpa-onnx/releases/download/speech-enhancement-models/gtcrn_simple.onnx
cp ../../../tests/fixtures/silero_vad.onnx .
```

Then run every app, or one:

```bash
./run.sh
./run.sh sherpa_bench
```

Results land in `results/<package>.jsonl`, one JSON line per scenario, plus
`results/<app>.size.json` with the bundle size. The `vad` package downloads
its model from a CDN at runtime, so that app needs network access.
