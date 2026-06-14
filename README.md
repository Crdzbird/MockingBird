# Auris

A native C++ audio capture + detection engine with a pure **C ABI**, designed to be
embedded from many languages (Dart/Flutter, Kotlin, Swift, Go, .NET, Python, Rust,
Node, WASM). Auris captures microphone audio, optionally denoises it, detects
**human voice** (multilingual) and **non-speech acoustic events**, classifies what it
hears, and streams only the relevant audio out as encoded packets.

> Status: capture → preprocess → detect → encode runs **live and verified on
> Android, iOS, and macOS**. Core is fuzzed, stress-tested, fault-injected, and
> clean under AddressSanitizer / UndefinedBehaviorSanitizer / ThreadSanitizer.

---

## What it does

| Stage | Component | Notes |
|---|---|---|
| Capture | miniaudio / Oboe (Android) / CoreAudio (Apple) | int16 mono, 8/16/32/48 kHz |
| Preprocess *(optional)* | **RNNoise** | recurrent noise suppression; bridged to its native 48 kHz |
| Voice detection | **libfvad** (fast) or **Silero v5** via ONNX Runtime (neural) | language-agnostic |
| Anomaly gate | RMS + spectral-flux + centroid z-scores (PFFFT) | adaptive rolling background |
| Sound classification | **YAMNet** via ONNX Runtime | all 521 AudioSet classes |
| Encode | **libopus** / PCM (FLAC planned) | per-segment |

Detection is one OR'd trigger: VAD or anomaly opens a *segment*; packets flow only
while a segment is open, so the host receives speech/events, not the silence between.

## Architecture

```
 T1  RT capture thread        alloc-free, never blocks
        │  SPSC PCM ring       lock-free, fixed size, drop-newest on full
        ▼
 T2  Worker thread            preprocess → VAD + anomaly + classify → encode
        │  bounded packet pool drop-oldest on full, raises AURIS_EVT_OVERRUN
        ▼
 T3  Host drain thread        auris_drain / auris_release at any pace
```

The two queues are the **only** coupling between real-time work and the host. A slow
host just drains slower; T1/T2 never feel backpressure. The C ABI surface
(`include/auris/auris.h`) is the contract: opaque handle, POD structs, status codes,
no C++/exceptions/STL across the boundary, zero-copy borrowed packets with an explicit
release.

## Repo layout

```
include/auris/auris.h     the C ABI (the contract — bindings generate from this)
src/                      engine, detectors, encoders, capture backends, preprocess
third_party/              vendored, permissive-only deps (see Licensing)
tests/                    79 doctest cases incl. fuzz / stress / fault-injection
packages/                 Flutter federated plugin (auris + per-platform + example)
```

---

## Building the core (CMake)

```bash
cmake -S . -B build -DAURIS_ENABLE_TESTS=ON \
      -DAURIS_ENABLE_NEURAL_VAD=ON          # Silero + YAMNet (needs ONNX Runtime)
cmake --build build -j
./build/test_auris                          # 79 cases
```

Useful options (all default to a sane value):

| Option | Default | Effect |
|---|---|---|
| `AURIS_ENABLE_TESTS` | OFF | build the doctest suite |
| `AURIS_ENABLE_NEURAL_VAD` | OFF | compile Silero VAD + YAMNet (ONNX Runtime) |
| `AURIS_ENABLE_RNNOISE` | OFF | compile RNNoise preprocessing |
| `AURIS_ENABLE_OPUS` | ON | compile the Opus encoder |
| **`AURIS_ENABLE_LOG`** | **OFF** | emit internal diagnostics to stderr/logcat. **Release builds are silent by default** — turn this on only to debug. |

### Sanitizers

```bash
cmake -S . -B build-asan -DAURIS_ENABLE_TESTS=ON \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build-asan -j && ./build-asan/test_auris

cmake -S . -B build-tsan -DAURIS_ENABLE_TESTS=ON \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
      -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"
cmake --build build-tsan -j && ./build-tsan/test_auris
```

## Minimal C usage

```c
auris_config_t cfg;
auris_config_defaults(&cfg);
cfg.backend = AURIS_BACKEND_HOST_PCM;   // or AUTO to use the OS mic
cfg.enable_vad = 1;

auris_engine_t* e = NULL;
if (auris_create(&cfg, &e) != AURIS_OK) { /* handle */ }
auris_start(e);

/* drain loop (your thread) */
auris_packet_t pkt;
while (auris_drain(e, &pkt) == AURIS_OK) {
    /* forward pkt.data[0..pkt.size) ... then: */
    auris_release(e, &pkt);             // borrowed — release promptly
}
auris_event_t ev;
while (auris_poll_event(e, &ev) == AURIS_OK) { /* SPEECH_BEGIN, ANOMALY, ... */ }

auris_stop(e);
auris_destroy(e);
```

Ownership rules are enforced and tested: every `auris_create` pairs with one
`auris_destroy`; packet `data` is borrowed until the matching `auris_release`; no C++
exception ever crosses the ABI; invalid input returns a status, never crashes.

## Flutter

The `packages/` directory is a federated plugin (flat VGV-style layout). The Dart side
reaches the C ABI directly via `dart:ffi`; there are no platform channels on the hot
path. See `packages/auris/example` for a working demo (sensitivity dial, multilingual
voice indicator, RNNoise toggle, live event log).

```dart
final engine = AurisEngine.create(AurisConfig.sensitive(
  0.6,                                          // one 0..1 dial drives every detector
  vadTier: auris_vad_tier.AURIS_VAD_NEURAL,     // multilingual
  neuralVadModelPath: sileroPath,
  soundClassModelPath: yamnetPath,
  enableRnnoise: true,
));
```

## Go

`bindings/go` is a cgo binding over the same C ABI, with an idiomatic API:
detections arrive on Go channels. Build `libauris` (CMake) once, then `go test`.
See [`bindings/go/README.md`](bindings/go/README.md).

```go
e, _ := auris.New(auris.Sensitive(0.6,
    auris.WithNeuralVADModel("silero_vad.onnx"),
    auris.WithSoundClassModel("yamnet_3s.onnx")))
e.Start()
for ev := range e.Events() {
    if ev.Type == auris.EventSpeechBegin { /* voice, any language */ }
}
```

## Cross-binding conformance

[`examples/conformance`](examples/conformance) is a bidirectional Go ⇄ Dart
differential test: each peer feeds deterministic audio to the other, processes
it through its own binding over the shared engine, and verifies the two produce
**byte-identical** encoded packets, events, and timestamps — proving the
language surfaces stay in sync.

```bash
cd examples/conformance && ./run.sh   # → ✅ CONFORMANCE PASS … both ways
```

---

## Platform integration notes

These are the non-obvious requirements that make the native library link and capture
correctly on each platform. They are encoded in the build files under `packages/` —
this section explains *why*, so they don't get accidentally removed.

### Android (`packages/auris_android`)
- **NDK r28+** and `-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384` — 16 KB
  page alignment required by Android 15+. Oboe ≥ 1.10 and ONNX Runtime ≥ 1.26 ship
  16 KB-aligned `.so`s.
- **Oboe** via Prefab (`find_package(oboe CONFIG)`); **ONNX Runtime** via a glob of the
  Gradle transform cache + `SHARED IMPORTED` (the AAR has no Prefab metadata).
- `abiFilters arm64-v8a, x86_64`.
- The shared C/C++ lives at the repo root and is reached via `AURIS_ROOT`. AGP marks the
  native build `upToDateWhen=false`, so out-of-tree edits *are* picked up by a plain
  `flutter build` (no cache clear needed) — see the note in `android/build.gradle`.

### iOS & macOS (`packages/auris_ios`, `packages/auris_macos`)
- A podspec `prepare_command` rsyncs the repo's C/C++ tree into the pod, and
  `source_files` globs compile it inline so symbols land in the host app.
- **Static linkage**: `use_frameworks! :linkage => :static` (ONNX ships a static
  xcframework that CocoaPods otherwise rejects).
- **FFI symbol export (critical).** `DynamicLibrary.process()` searches the executable's
  *dynamic symbol table*. With static linkage the linker keeps only link-time-referenced
  objects, and the C ABI is only referenced at runtime from Dart — so without help the
  symbols are dropped and lookups fail with *"symbol not found"*. Two pieces fix it:
  1. `AurisPlugin.register()` (always called by Flutter) references the ABI under a
     never-true guard, anchoring its transitive closure into the binary.
  2. The podspec adds `-Wl,-export_dynamic` to the **consumer** target, publishing those
     default-visibility symbols into the dynamic table.
- **Microphone permission & capture:**
  - **iOS** RemoteIO can't capture until an `AVAudioSession` (record category) is
    configured and activated — done in the capture backend. `permission_handler` needs
    `PERMISSION_MICROPHONE=1` in the app Podfile, or it reports `denied` unconditionally.
  - **macOS** a `HALOutput` unit must be bound to the default *input* device
    (`kAudioOutputUnitProperty_CurrentDevice`), or it captures silence. miniaudio's path
    doesn't request authorization, so the demo calls `AVCaptureDevice.requestAccess`
    up front (the app carries the audio-input entitlement + usage string).

### Bundled models
`silero_vad.onnx` (v5.1.2, ~2 MB), `yamnet_3s.onnx` (~15 MB), and RNNoise's model
(baked into `rnn_data.c`). The Silero/YAMNet models ship as Flutter assets and are
extracted to an app-writable path at runtime.

---

## Licensing

Auris is permissive-only — **no GPL/LGPL**. Vendored dependencies:

| Dep | License |
|---|---|
| miniaudio | public domain / MIT-0 |
| libfvad | BSD-3-Clause (WebRTC) + MIT |
| PFFFT | BSD-like (FFTPACK-derived) |
| libopus | BSD-3-Clause |
| RNNoise | BSD-3-Clause (Xiph) |
| ONNX Runtime | MIT (linked, not vendored) |
| doctest | MIT (test only) |
