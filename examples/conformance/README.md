# Cross-binding conformance harness (Go ⇄ Dart ⇄ Python)

Proves that the **Go**, **Dart/Flutter**, and **Python** bindings — all driving
the same native `libauris` — turn identical input audio into **byte-identical**
output: same encoded audio (quality), same events (information), same timestamps
and stats (sync). It's a bidirectional cross-language differential test, run
across every pair.

```
./run.sh
```
```
════════ Go <-> Dart ════════
[go]   generated 32000 samples; my engine: 100 packets / 6 events / 7656 bytes, digest 0x88587912845f408a
[dart] received 32000 samples from go; generator parity MATCH ✓ — processing through MY binding…
[dart] my result for go's audio: 100 packets / 6 events / 7656 bytes, digest 0x88587912845f408a
[go]   parity go-audio→dart: MATCH ✓   |   dart-audio→go: MATCH ✓
-------- Go <-> Dart: PASS
════════ Go <-> Python ════════      -------- Go <-> Python: PASS
════════ Python <-> Dart ════════    -------- Python <-> Dart: PASS

✅ conformance passed — every binding pair is byte-identical
```

Run a subset with `PAIRS="go-python" ./run.sh`, or move the base port with
`PORT=9931 ./run.sh`.

## How it works

Two peers connect over TCP per run: one **listens**, one **dials**. The Go peer
([`go/`](go)) listens, the Dart peer ([`dart/`](dart)) dials, and the Python peer
([`python/`](python)) does **either**, which is what makes all three pairings
work. The Dart peer uses the exact FFI bindings the Flutter plugin ships (loaded
via `DynamicLibrary.open` rather than static linkage); the Python peer uses the
ctypes binding.

The seeds belong to the **role**, not the language: the listener always generates
from `0xA1A1…` and the dialer from `0xB2B2…`. That keeps every pairing
compatible without any peer needing to know which language is on the other end.

Each peer, symmetrically:

1. **Generates** a deterministic 2 s / 16 kHz signal from an integer-only
   `xorshift64` (no floats — so the samples are bit-identical across languages),
   using its own seed.
2. **Processes its own** signal through its binding → the *expected* result.
3. **Sends** its raw PCM to the peer; **receives** the peer's PCM.
4. **Generator parity check**: the received PCM must equal a local regeneration
   from the peer's seed — isolates a generator bug from a processing bug.
5. **Processes the peer's** signal through its binding → the *computed* result.
6. **Exchanges** result digests and cross-checks **both directions**:
   - did the peer process *my* audio identically to me?
   - did *I* process the peer's audio identically to the peer?

Both checks must pass on both peers ⇒ full bidirectional parity.

### What's compared

A single **FNV-1a/64** digest over a fixed big-endian canonical encoding of the
complete output:

- every **packet**: `segment_id, pts_us, capture_ts_us, flags, codec, size`, and
  the raw encoded **bytes** (the Opus payload — the actual audio quality);
- every **event**: `type, segment_id, timestamp_us, reason_mask, confidence`
  (IEEE-754 bits) — the detection information;
- **stats**: `frames_captured, packets_emitted, segments_opened/closed`.

### Determinism contract

Both sides use an identical pipeline so the engine output is a pure function of
the input:

| | |
|---|---|
| signal | integer `xorshift64`, 32000 samples, periodic loud bursts |
| config | HOST_PCM · 16 kHz mono · 20 ms · **Opus** · fast VAD + anomaly · ring 2¹⁷ · queue 4096 |
| order | push every frame → `Stop` (flushes the tail) → drain all |
| digest | FNV-1a/64, big-endian, fixed field order |

This works because the engine is deterministic for a given input: Opus encoding,
libfvad VAD, and the anomaly gate are pure functions of the sample sequence, and
**packet/event timestamps are derived from a per-frame counter**
(`base + frame_index × frame_us`) rather than wall-clock or last-push time — so
they don't depend on the producer/worker race. (Bringing this harness up is
exactly what surfaced that timestamp determinism fix; see
`tests/test_host_pcm.cpp` "per-frame timestamps are deterministic".)

## Running manually

Any listener paired with any dialer:

```bash
# listener (pick one)
cd go     && go run . -addr 127.0.0.1:9999 -peer python
python3 python/conformance.py listen 9999 dart

# dialer (pick one)
cd dart   && dart pub get && dart run conformance.dart 127.0.0.1:9999 --peer=go
python3 python/conformance.py dial 127.0.0.1:9999 go
```

The trailing peer name is cosmetic — it only labels the log output.

Diagnostics:

```bash
go run . -self                       # determinism: same signal twice → same digest
python3 python/conformance.py self   # ...from Python

go run . -dump                       # per-packet / per-event detail
dart run conformance.dart dump       # ...from Dart
python3 python/conformance.py dump   # ...from Python
```

`diff` two dumps to localize a divergence to a specific field — that is exactly
how the non-deterministic packet timestamps were found and fixed.

Every peer auto-locates `build/libauris.{dylib,so}`. Override with an explicit
path (Dart: second positional argument; Python: the `AURIS_LIBRARY_PATH`
environment variable).
