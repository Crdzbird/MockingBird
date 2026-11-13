# Mockingbird for Go

Go bindings for the [Mockingbird](../../README.md) native audio engine over its
C ABI: capture, optional RNNoise, multilingual voice detection, an anomaly gate,
YAMNet sound classification, and Opus or PCM packets, delivered on channels.

```go
import mockingbird "github.com/Crdzbird/MockingBird/bindings/go"
```

The package name is `mockingbird`; the explicit name in the import makes that
visible, since the path ends in `go`.

## Prerequisites

The binding links `libmockingbird`, built from this repository with CMake:

```bash
cmake -S . -B build -DMOCKINGBIRD_BUILD_SHARED=ON -DMOCKINGBIRD_ENABLE_NEURAL_VAD=ON
cmake --build build -j
```

The cgo directives in [`mockingbird.go`](mockingbird.go) point at the
repository's `include/` and `build/` directories, so a clone of
https://github.com/Crdzbird/MockingBird needs no further setup. With `go get` or
an installed copy of the library, override the flags:

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

`DefaultConfig` runs fast VAD and the anomaly gate on the platform's microphone.
Everything else is grouped by concern:

| Field | Type |
|---|---|
| `VAD` | `VadConfig`: tier, thresholds, hangover, model path |
| `Anomaly` | `AnomalyConfig`: z-score triggers, background window |
| `SoundClass` | `SoundClassConfig`: threshold, cadence, model path |
| `Segments` | `SegmentConfig`: pre-roll, post-roll, max length |
| `Encoder` | `EncoderConfig`: `OpusEncoder()` for Opus, `OggOpusEncoder()` for Ogg Opus |
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

The packet that closes a segment has `IsLast()` set. Concatenate a segment's
payloads in order: with `CodecOggOpus` they form an `.opus` file, and with
`CodecPCMS16LE`, `WAV(pcm, 16000, 1)` returns a WAV file. A segment cut short by
`Stop` ends with an `IsLast()` packet only under Ogg Opus; `EventSegmentEnd`
arrives either way.

## Microphone

`Config.VoiceProcessing` (`VoiceAll`, or `VoiceEchoCancel`, `VoiceGainControl`
and `VoiceNoiseSuppress`) asks the platform for its voice processing;
`Stats.Capture` reports what applied and how often capture restarted.
`InputDevices(backend)` lists microphones and `Engine.SelectInputDevice`
chooses one before `Start`. Interruptions and route changes arrive as
`EventCaptureInterrupted`, `EventCaptureResumed` and `EventRouteChanged`.

## Playback

`NewDecoder(rate, channels)` turns the packets of a `CodecOpus` engine, or of
any Opus encoder, back into interleaved int16 PCM. One decoder per stream, its
packets in order; a packet that never arrived goes through `Conceal(frames)`
instead, where 0 frames means the last packet's duration:

```go
d, err := mockingbird.NewDecoder(16000, 1)
if err != nil {
    log.Fatal(err)
}
defer d.Close()
pcm, err := d.Decode(pkt.Data)
```

`NewPlayer` plays Opus or PCM packets on the device's output through a jitter
buffer ordered by pts. It starts from `DefaultPlayerConfig` (16 kHz mono,
60 ms target delay) and takes options: `WithPlayerBackend`, `WithPlayerRate`,
`WithPlayerChannels`, `WithTargetDelay`, `WithMaxDelay`, `WithConcealMax`,
`WithQueuePackets` and `WithPlayerConfig`:

```go
p, err := mockingbird.NewPlayer(mockingbird.WithTargetDelay(80 * time.Millisecond))
if err != nil {
    log.Fatal(err)
}
defer p.Close()
p.Start()
for pkt := range e.Packets() {
    p.PushPacket(pkt)
}
```

`Push(codec, data, ptsUs, flags)` copies one packet and never blocks;
`PushPacket` places an engine packet at its `CaptureTsUs`, one clock for the
whole run, since `PtsUs` restarts with every segment;
`PacketFlagLast` ends a talkspurt so the player goes quiet without counting an
underrun. Missing packets are concealed, late and duplicate ones dropped.
`Pause`, `Resume`, `Stop`, `Flush`, `SetVolume` and `SetMuted` control it, and
`Stats` returns `PlayerStats` with a `PlayerState`. With `BackendHostPCM` no
device opens: `Read(frames)` renders the output for the host to play itself.

## Ownership and threading

Every method is safe from any goroutine; concurrent `PushPCM` calls are
serialised for the single-producer ring. `Close` stops, releases the native
engine and closes both channels, which ends a `for ev := range e.Events()` loop;
`Stop` keeps them open so the engine can restart. An internal goroutine drains
the native queues every 2 ms and drops, never blocks, when a channel is full;
`Dropped` reports the count.

Native failures are `*StatusError`:

```go
var se *mockingbird.StatusError
if errors.As(err, &se) && se.Status == mockingbird.StatusErrPermission {
    ...
}
```

Methods on a closed engine, decoder or player return `mockingbird.ErrClosed`.

## Testing

```bash
go test ./...
go test -race ./...
```

The [example](example) is a flag-driven command that runs any combination of
backend, detectors, denoise and codec, plus named `-preset`s.
