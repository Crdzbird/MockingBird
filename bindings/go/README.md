# Mockingbird for Go

Go bindings for the [Mockingbird](../../README.md) native audio engine over its C
ABI: capture, optional RNNoise, multilingual voice detection, an anomaly gate,
YAMNet sound classification, and Opus or PCM packets, delivered on channels.

```go
import "github.com/Crdzbird/MockingBird/bindings/go"
```

## Prerequisites

The binding links `libmockingbird`, built from this repository with CMake:

```bash
cmake -S . -B build -DMOCKINGBIRD_BUILD_SHARED=ON -DMOCKINGBIRD_ENABLE_NEURAL_VAD=ON
cmake --build build -j
```

The cgo directives in [`mockingbird.go`](mockingbird.go) point at the repository's
`include/` and `build/` directories, so a checkout needs no further setup.
For an installed copy, override the flags:

```bash
CGO_CFLAGS="-I/usr/local/include" \
CGO_LDFLAGS="-L/usr/local/lib -lmockingbird -Wl,-rpath,/usr/local/lib" go build ./...
```

## Quick start

```go
e, err := mockingbird.New(mockingbird.DefaultConfig())
if err != nil {
    log.Fatal(err)
}
defer e.Close()
if err := e.Start(); err != nil {
    log.Fatal(err)
}
for ev := range e.Events() {
    switch ev.Type {
    case mockingbird.EventSpeechBegin:
        fmt.Println("voice")
    case mockingbird.EventSoundClass:
        fmt.Println(ev.Category(), ev.Confidence)
    }
}
```

`DefaultConfig` runs fast VAD and the anomaly gate on the platform's
microphone. Everything else is grouped by concern:

| Field | Type |
|---|---|
| `VAD` | `VadConfig`: tier, thresholds, hangover, model path |
| `Anomaly` | `AnomalyConfig`: z-score triggers, background window |
| `SoundClass` | `SoundClassConfig`: threshold, cadence, model path |
| `Segments` | `SegmentConfig`: pre-roll, post-roll, max length |
| `Encoder` | `EncoderConfig`: `OpusEncoder()` for Opus |
| `Queues` | `QueueConfig`: ring sizes and the ungated PCM tap |
| `Performance` | `PerformanceConfig`: silence gate, degradation, wake batching; `MobilePerformance()` |

### One sensitivity dial

`Sensitive` turns every detector on with neural VAD and derives all thresholds
from one `0..1` value; options adjust the rest:

```go
cfg := mockingbird.Sensitive(0.6,
    mockingbird.WithNeuralVADModel("silero_vad.onnx"),
    mockingbird.WithSoundClassModel("yamnet_3s.onnx"),
    mockingbird.WithDenoise(true),
    mockingbird.WithEncoder(mockingbird.OpusEncoder()),
)
```

### Host-pushed PCM

```go
cfg := mockingbird.DefaultConfig()
cfg.Backend = mockingbird.BackendHostPCM
e, _ := mockingbird.New(cfg)
e.Start()
e.PushPCM(samples, captureTsUs)
```

### Live retuning

```go
cfg.Anomaly.RMSZ = 2.0
err := e.UpdateConfig(cfg)
```

Detector state survives. Structural fields must match the creating config.

## Events

`Event` is one struct; its accessors interpret the payload for the types that
carry one: `Category()`, `IsHumanVoice()`, `AnomalyReasons()`,
`DegradationLevel()`, `BusyFraction()`, `BackendError()`. `Packet.Data` is a
Go-owned copy, with `IsPreroll()`, `IsPostroll()`, `IsKeyframe()` and
`IsLast()`. `Stats` nests `WorkerHealth`.

## Ownership and threading

Every method is safe from any goroutine; concurrent `PushPCM` calls are
serialised for the single-producer ring. `Close` stops, releases the native
engine and closes both channels, which ends a `for ev := range e.Events()`
loop; `Stop` keeps them open so the engine can restart. An internal goroutine
drains the native queues every 2 ms and drops, never blocks, when a channel is
full; `Dropped` reports the count.

Native failures are `*StatusError`:

```go
var se *mockingbird.StatusError
if errors.As(err, &se) && se.Status == mockingbird.StatusErrPermission {
    ...
}
```

Methods on a closed engine return `mockingbird.ErrClosed`.

## Testing

```bash
go test ./...
go test -race ./...
```

The [example](example) is a flag-driven command that runs any combination of
backend, detectors, denoise and codec, plus named `-preset`s.
