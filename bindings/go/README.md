# Auris for Go

Idiomatic Go bindings for the [Auris](../../README.md) native audio capture +
detection engine, via cgo over its pure C ABI. Capture → optional RNNoise →
multilingual voice + anomaly + YAMNet detection → encoded packets, exposed as Go
channels.

```go
import "github.com/auris-audio/auris"
```

## Prerequisites

The binding links **`libauris`**, built from this repository with CMake:

```bash
# from the repo root
cmake -S . -B build -DAURIS_BUILD_SHARED=ON \
      -DAURIS_ENABLE_NEURAL_VAD=ON           # optional: Silero VAD + YAMNet (needs ONNX Runtime)
cmake --build build -j
```

This produces `build/libauris.<so|dylib>`. The cgo directives in
[`auris.go`](auris.go) reference the repo-relative `include/` and `build/`
directories and bake an rpath to `build/`, so from a checkout it works with no
extra setup:

```bash
cd bindings/go
go test ./...
```

To link a `libauris` installed elsewhere, override at build time:

```bash
CGO_CFLAGS="-I/usr/local/include" \
CGO_LDFLAGS="-L/usr/local/lib -lauris -Wl,-rpath,/usr/local/lib" \
go build ./...
```

> If you built `libauris` with ONNX Runtime, it carries an absolute load path to
> `libonnxruntime`, so Go needs no extra link flags for it — just keep
> `libonnxruntime` installed where `libauris` expects it.

## Quick start

```go
package main

import (
    "fmt"
    "github.com/auris-audio/auris"
)

func main() {
    e, err := auris.New(auris.DefaultConfig()) // default mic, fast VAD + anomaly
    if err != nil { panic(err) }
    defer e.Close()
    if err := e.Start(); err != nil { panic(err) }

    for ev := range e.Events() {
        switch ev.Type {
        case auris.EventSpeechBegin:
            fmt.Println("voice detected")
        case auris.EventSoundClass:
            fmt.Printf("sound: %s\n", auris.CategoryForYAMNetClass(ev.SoundClass()))
        }
    }
}
```

See [`example/`](example) for a complete live-mic demo (`go run ./example`).

### One sensitivity dial

`Sensitive` maps a single `0..1` value onto every detector at once (mirrors the
Dart `AurisConfig.sensitive`), defaulting to multilingual neural VAD:

```go
cfg := auris.Sensitive(0.6,
    auris.WithNeuralVADModel("silero_vad.onnx"),
    auris.WithSoundClassModel("yamnet_3s.onnx"),
    auris.WithRNNoise(true),
)
e, _ := auris.New(cfg)
```

### Host-pushed PCM

For a host that already has audio (no internal capture):

```go
cfg := auris.DefaultConfig()
cfg.Backend = auris.BackendHostPCM
e, _ := auris.New(cfg); e.Start()
e.PushPCM(int16Samples, captureTsUs)   // 16-bit mono, your monotonic timestamp
```

## API

| | |
|---|---|
| `New(Config) (*Engine, error)` | construct; validates every field |
| `(*Engine).Start() / Stop() / Close()` | lifecycle; `Close` is idempotent and releases everything |
| `(*Engine).PushPCM([]int16, ts)` | feed PCM (HOST_PCM backend) |
| `(*Engine).Events() <-chan Event` | speech / anomaly / segment / sound-class / overrun events |
| `(*Engine).Packets() <-chan Packet` | encoded audio; each `Packet.Data` is a Go-owned copy |
| `(*Engine).Stats() (Stats, error)` | observability counters |
| `(*Engine).Dropped() (events, packets uint64)` | counts surplus dropped when a channel was full |
| `DefaultConfig()` / `Sensitive(dial, …Option)` | build a `Config` |
| `CategoryForYAMNetClass(int)` / `IsHumanVoice(int)` / `IsSpeechClass(int)` | interpret sound-class events |

### Ownership & threading

- **All methods are goroutine-safe.** `Start`/`Stop`/`Close`/`PushPCM`/`Stats`
  are internally synchronized; concurrent `PushPCM` from multiple goroutines is
  serialized to satisfy the single-producer PCM ring. The `Events`/`Packets`
  channels may be read from any goroutine.
- **`Close` is mandatory.** It stops, destroys native resources, and *closes*
  the channels — which is what terminates a `for ev := range e.Events()` loop.
  `Stop` alone leaves the channels open (so you can restart). A finalizer is a
  best-effort backstop, not a substitute.
- **Packet bytes are copied** into Go before the native borrow is released, so
  they're GC-safe to keep.
- **The consumer never stalls the engine.** An internal goroutine drains the
  native queues on a fixed 2 ms poll; if your consumer can't keep up, surplus is
  dropped at the channel (count it with `Dropped()`) — the same drop-oldest
  philosophy the native side uses, also surfaced as `EventOverrun`.

### Errors

Native failures are `*StatusError`, so you can branch on the cause:

```go
if err := e.Start(); err != nil {
    var se *auris.StatusError
    if errors.As(err, &se) && se.Status == auris.StatusErrPermission {
        // microphone access denied
    }
}
```

Methods on a closed engine return `auris.ErrClosed`.

## Testing

```bash
go test ./...          # functional
go test -race ./...    # concurrency (pump goroutine + channels)
```

The suite covers the category mapping, the sensitivity dial, config validation,
HOST_PCM passthrough, stats accounting, lifecycle churn, and a real-voice
differential against `tests/fixtures/hello.wav`.
