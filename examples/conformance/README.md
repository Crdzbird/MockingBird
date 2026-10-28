# Cross-binding conformance harness

This harness checks that the Go, Dart and Python bindings, all driving the same
native `libmockingbird`, turn identical input audio into byte-identical output:
the same encoded audio, the same events, and the same timestamps and stats. It
runs in both directions across every pair of bindings.

```bash
./run.sh
```

A passing run ends like this:

```
======== Go <-> Dart (127.0.0.1:9999) ========
[go]   generated 32000 samples; my engine: 100 packets / 6 events / 7656 bytes, digest 0x88587912845f408a
[dart] received 32000 samples from go; generator parity MATCH, processing through MY binding...
[dart] my result for go's audio: 100 packets / 6 events / 7656 bytes, digest 0x88587912845f408a
[go]   parity go-audio to dart: MATCH   |   dart-audio to go: MATCH
-------- Go <-> Dart: PASS
...
conformance passed: every binding pair is byte-identical
```

Run a subset with `PAIRS="go-python" ./run.sh`, or move the base port with
`PORT=9931 ./run.sh`.

## How it works

Each run connects two peers over TCP: one listens and one dials. The Go peer in
[`go/`](go) listens, the Dart peer in [`dart/`](dart) dials, and the Python peer
in [`python/`](python) can do either, which is what makes all three pairings
possible. The Dart peer uses the same FFI bindings the Flutter plugin ships,
loaded with `DynamicLibrary.open` instead of static linkage. The Python peer
uses the ctypes binding.

The seeds belong to the role, not the language. The listener always generates
from `0xA1A1...` and the dialer from `0xB2B2...`, so no peer needs to know which
language is on the other end.

Each peer does the same six steps:

1. Generate a 2 s, 16 kHz signal from its own seed with an integer-only
   `xorshift64`. There is no floating point, so the samples are bit-identical in
   every language.
2. Process its own signal through its binding. This is the expected result.
3. Send its raw PCM to the other peer and receive the other peer's PCM.
4. Check generator parity: the received PCM must equal a local regeneration from
   the other peer's seed. A failure here points at the signal generator, not at
   the engine.
5. Process the other peer's signal through its own binding. This is the computed
   result.
6. Exchange result digests and compare both directions: did the other peer
   process my audio the way I did, and did I process its audio the way it did?

The run passes only when both checks pass on both peers.

### What is compared

One FNV-1a/64 digest over a fixed big-endian encoding of the whole output:

- every packet: `segment_id`, `pts_us`, `capture_ts_us`, `flags`, `codec`,
  `size`, and the encoded bytes themselves, so audio quality is covered;
- every event: `type`, `segment_id`, `timestamp_us`, `reason_mask`, and
  `confidence` as IEEE-754 bits;
- the stats: `frames_captured`, `packets_emitted`, `segments_opened` and
  `segments_closed`.

### Determinism contract

Every peer runs the same pipeline, so the engine output depends only on the
input:

| Item | Value |
|---|---|
| Signal | integer `xorshift64`, 32000 samples, periodic loud bursts |
| Config | host PCM, 16 kHz mono, 20 ms frames, Opus, fast VAD and the anomaly gate, ring of 2^17 frames, queue of 4096 packets |
| Order | push every frame, then stop (which flushes the tail), then drain everything |
| Digest | FNV-1a/64, big-endian, fixed field order |

This works because the engine is deterministic for a given input. Opus encoding,
the libfvad detector and the anomaly gate are pure functions of the sample
sequence, and packet and event timestamps come from a per-frame counter (`base +
frame_index * frame_us`) instead of the wall clock or the time of the last push.
They therefore do not depend on how the producer and the worker thread
interleave. Building this harness is what exposed the original timestamp bug;
the regression test is "per-frame timestamps are deterministic" in
`tests/test_host_pcm.cpp`.

## Running peers by hand

Any listener works with any dialer:

```bash
# listener, pick one
cd go && go run . -addr 127.0.0.1:9999 -peer python
python3 python/conformance.py listen 9999 dart

# dialer, pick one
cd dart && dart pub get && dart run conformance.dart 127.0.0.1:9999 --peer=go
python3 python/conformance.py dial 127.0.0.1:9999 go
```

The trailing peer name only labels the log output.

For diagnosis:

```bash
go run . -self                       # the same signal twice must give the same digest
python3 python/conformance.py self   # the same check from Python

go run . -dump                       # per-packet and per-event detail
dart run conformance.dart dump
python3 python/conformance.py dump
```

`diff` two dumps to find the first field that diverges. That is how the
non-deterministic packet timestamps were found and fixed.

Every peer looks for `build/libmockingbird.dylib` or `build/libmockingbird.so`
on its own. To use another path, pass it as the Dart peer's second positional
argument, or set `MOCKINGBIRD_LIBRARY_PATH` for the Python peer.
