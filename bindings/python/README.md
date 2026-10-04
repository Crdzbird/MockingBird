# Auris for Python

Python bindings for the [Auris](../../README.md) native audio engine: capture,
optional RNNoise, multilingual voice detection, an anomaly gate, YAMNet sound
classification, and Opus or PCM packets. Pure ctypes: no compiler, no build
step, no runtime dependency, CPython 3.10 or newer.

## Setup

Build the native library once from the repository root:

```bash
cmake -S . -B build -DAURIS_BUILD_SHARED=ON -DAURIS_ENABLE_NEURAL_VAD=ON
cmake --build build -j
```

A checkout finds `build/libauris.*` on its own. Elsewhere set
`AURIS_LIBRARY_PATH` or call `auris.load_library(path)` first.

```bash
cd bindings/python
python3 -m unittest discover -s tests -v
```

## Quick start

```python
import auris

with auris.Engine(auris.Config()) as engine:
    engine.start()
    for event in engine.events(timeout=10):
        match event:
            case auris.Event(type=auris.EventType.SPEECH_BEGIN):
                print("voice")
            case auris.Event(type=auris.EventType.SOUND_CLASS, category=c, confidence=p):
                print(c, p)
```

`Config()` runs fast VAD and the anomaly gate on the platform's microphone.
Everything else is grouped by concern:

| Field | Class |
|---|---|
| `vad` | `VadConfig`: tier, thresholds, hangover, model path |
| `anomaly` | `AnomalyConfig`: z-score triggers, background window |
| `sound_class` | `SoundClassConfig`: threshold, cadence, model path |
| `segments` | `SegmentConfig`: pre-roll, post-roll, max length |
| `encoder` | `EncoderConfig`: `EncoderConfig.opus()` for Opus |
| `queues` | `QueueConfig`: ring sizes and the ungated PCM tap |
| `performance` | `PerformanceConfig`: silence gate, degradation, wake batching; `.mobile()` |

### One sensitivity dial

```python
cfg = auris.sensitive(
    0.6,
    neural_vad_model_path="silero_vad.onnx",
    sound_class_model_path="yamnet_3s.onnx",
    denoise=True,
    encoder=auris.EncoderConfig.opus(),
)
```

### Process a file

```python
cfg = auris.Config(backend=auris.Backend.HOST_PCM)
with auris.Engine(cfg) as engine:
    engine.start()
    for i in range(len(pcm) // 320):
        engine.push_pcm(pcm[i * 320:(i + 1) * 320], i * 20_000)
    packets = engine.drain_packets()
```

`push_pcm` accepts `bytes`, `array('h')`, `memoryview`, numpy `int16` arrays,
or a list of ints.

### Live retuning

```python
cfg.anomaly.rms_z = 2.0
engine.update_config(cfg)
```

### The PCM tap and level metering

```python
cfg = auris.Config(queues=auris.QueueConfig(pcm_tap=True))
raw = engine.read_pcm(4096)
level = engine.stats().input_rms_dbfs
```

## Events, packets, stats

`Event` is one frozen dataclass whose properties interpret the payload:
`category`, `is_human_voice`, `anomaly_reasons`, `degradation_level`,
`busy_fraction`, `backend_error`. Match on `type` with keyword patterns, as
above. `Packet.data` is bytes you own, with `is_preroll`, `is_postroll`,
`is_keyframe` and `is_last`. `Stats.worker` is a `WorkerHealth`.

Errors raise `AurisError`, whose `status` is a `Status`:

```python
try:
    engine.start()
except auris.AurisError as exc:
    if exc.status is auris.Status.ERR_PERMISSION:
        ...
```

## Example

[`example.py`](example.py) runs the engine on the microphone or a WAV file with
named presets:

```bash
python3 example.py --preset everything --file ../../tests/fixtures/hello.wav --tap
python3 example.py --preset music --file ../../tests/fixtures/miaow_16k.wav
python3 example.py --seconds 10 --sensitivity 0.8
```

## Threading

Drive one engine from one thread. Packet bytes are copied before the native
borrow is released, so they are safe to keep and forward.
