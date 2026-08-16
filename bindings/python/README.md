# Auris for Python

Python bindings for the [Auris](../../README.md) native audio engine: capture →
optional RNNoise → multilingual voice + anomaly detection + YAMNet sound
classification → encoded packets.

Pure **ctypes** — no compiler, no build step, no third-party runtime
dependency. Works with any CPython ≥ 3.10.

## Setup

Build the native library once from the repo root:

```bash
cmake -S . -B build -DAURIS_BUILD_SHARED=ON \
      -DAURIS_ENABLE_NEURAL_VAD=ON        # optional: Silero VAD + YAMNet
cmake --build build -j
```

The binding finds `build/libauris.*` automatically from a checkout. Elsewhere,
set `AURIS_LIBRARY_PATH=/path/to/libauris.so` or call
`auris.load_library("/path/to/libauris.so")` before creating an engine.

```bash
cd bindings/python
python3 -m unittest discover -s tests -v      # 15 tests
```

## Quick start

```python
import auris

with auris.Engine(auris.Config()) as engine:      # default mic, fast VAD + anomaly
    engine.start()
    for event in engine.events(timeout=10):
        if event.type is auris.EventType.SPEECH_BEGIN:
            print("voice detected")
        elif event.type is auris.EventType.SOUND_CLASS:
            print("sound:", auris.category_for_yamnet_class(event.sound_class))
```

`Engine` is a context manager, so the native handle is always released.

### One dial tunes everything

```python
cfg = auris.sensitive(
    0.6,                                       # 0 = conservative, 1 = eager
    neural_vad_model_path="silero_vad.onnx",   # multilingual neural VAD
    sound_class_model_path="yamnet_3s.onnx",
    enable_rnnoise=True,
    codec=auris.Codec.OPUS,
)
```

Any `Config` field can be passed as a keyword override.

### Process a file (no microphone)

```python
import array, wave, auris

with wave.open("speech.wav") as w:              # 16-bit mono
    pcm = array.array("h"); pcm.frombytes(w.readframes(w.getnframes()))

cfg = auris.Config(backend=auris.Backend.HOST_PCM)
with auris.Engine(cfg) as engine:
    engine.start()
    for i in range(len(pcm) // 320):
        engine.push_pcm(pcm[i*320:(i+1)*320], i * 20_000)
    for packet in engine.drain_packets():
        ...                                      # packet.data is bytes you own
```

`push_pcm` accepts anything buffer-like — `bytes`, `array('h')`, `memoryview`,
a numpy `int16` array — or a plain list of ints.

### Live retuning

Change detector thresholds without tearing the engine down (no model reload, no
gap in capture, detector state preserved):

```python
cfg.anomaly_rms_z = 2.0
engine.update_config(cfg)
```

### Continuous audio + level metering

Packets are segment-gated. Enable the tap for an **ungated** stream — useful to
archive everything or draw a waveform while still getting detections:

```python
cfg = auris.Config(enable_pcm_tap=True)
...
raw = engine.read_pcm(4096)          # little-endian int16 bytes
level = engine.stats().input_rms_dbfs   # dBFS, drives a level meter
```

## API

| | |
|---|---|
| `Engine(config)` / `.start()` / `.stop()` / `.close()` | lifecycle; also a context manager |
| `.push_pcm(samples, capture_ts_us)` | feed a `HOST_PCM` engine |
| `.poll_events()` / `.poll_event()` | drain detections |
| `.events(timeout=…)` | generator that polls for you |
| `.drain_packets()` / `.drain_packet()` | encoded audio (bytes copied for you) |
| `.read_pcm(max_samples)` | continuous ungated PCM tap |
| `.stats()` | counters + `input_rms_dbfs` / `input_peak_dbfs` |
| `.update_config(cfg)` | live retune |
| `Config(...)` / `sensitive(dial, **overrides)` | configuration |
| `category_for_yamnet_class` / `is_human_voice` / `is_speech_class` | interpret sound classes |

Errors raise `AurisError`, which carries a `Status` so you can branch:

```python
try:
    engine.start()
except auris.AurisError as exc:
    if exc.status is auris.Status.ERR_PERMISSION:
        ...   # microphone access denied
```

## Example

[`example.py`](example.py) is a flag-driven tour — live mic or a WAV file, any
detector combination, named presets:

```bash
python3 example.py --preset everything --file ../../tests/fixtures/hello.wav --tap
python3 example.py --preset music --file ../../tests/fixtures/miaow_16k.wav
python3 example.py --seconds 10 --sensitivity 0.8
```

## Threading

Drive one engine from one thread (the same single-consumer contract the C ABI
states for the drain side). Packet bytes are copied into Python before the
native borrow is released, so they are safe to keep and forward.
