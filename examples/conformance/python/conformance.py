#!/usr/bin/env python3
"""Python peer of the Auris cross-binding conformance harness.

Mirror of examples/conformance/{go/main.go,dart/conformance.dart}. It drives the
SAME native libauris through the ctypes binding, exchanges deterministic audio
with another peer over TCP, and proves both produce byte-identical packets,
events, and stats.

Unlike the Go peer (always listens) and the Dart peer (always dials), this one
takes EITHER role, so it can be paired with either of them:

    python3 conformance.py listen 9999          # acts as the listener
    python3 conformance.py dial 127.0.0.1:9999  # acts as the dialer
    python3 conformance.py dump                 # per-packet/event detail

The seeds belong to the ROLE, not the language: the listener always generates
from LISTENER_SEED and the dialer from DIALER_SEED, which is exactly what the Go
and Dart peers already do. That keeps every pairing compatible with no changes
to them.

The determinism contract (generator, config, processing order, and the FNV-1a/64
canonical digest) is identical to the other peers.
"""

from __future__ import annotations

import array
import socket
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "bindings" / "python"))

import auris  # noqa: E402

SAMPLE_RATE = 16000
FRAME_LEN = 320  # 20 ms @ 16 kHz
NUM_SAMPLES = 32000  # 2 s, 100 frames

# Role seeds — must match the Go peer's goSeed / dartSeed.
LISTENER_SEED = 0xA1A1A1A1A1A1A1A1
DIALER_SEED = 0xB2B2B2B2B2B2B2B2

U64 = (1 << 64) - 1


# --- deterministic signal (integer-only; identical to the other generators) ---


def gen_signal(seed: int) -> array.array:
    """xorshift64 with explicit 64-bit masking to match Go's uint64 semantics."""
    x = seed & U64
    out = array.array("h", bytes(NUM_SAMPLES * 2))
    for i in range(NUM_SAMPLES):
        x = (x ^ (x << 13)) & U64
        x ^= x >> 7
        x = (x ^ (x << 17)) & U64
        v = ((x >> 40) & 0x1FFF) - 4096  # noise in [-4096, 4095]
        if i % 8000 < 1600:  # 0.1 s loud burst every 0.5 s
            v += 11000 if ((i >> 3) & 1) == 0 else -11000
        out[i] = 32767 if v > 32767 else (-32768 if v < -32768 else v)
    return out


# --- FNV-1a/64 over a fixed big-endian canonical encoding ---------------------


class Hasher:
    __slots__ = ("h",)
    _PRIME = 0x100000001B3

    def __init__(self) -> None:
        self.h = 0xCBF29CE484222325

    def bytes_(self, data: bytes) -> None:
        h = self.h
        for b in data:
            h = ((h ^ b) * self._PRIME) & U64
        self.h = h

    def u32(self, v: int) -> None:
        self.bytes_(struct.pack(">I", v & 0xFFFFFFFF))

    def u64(self, v: int) -> None:
        self.bytes_(struct.pack(">Q", v & U64))


def float32_bits(f: float) -> int:
    return struct.unpack("<I", struct.pack("<f", f))[0]


# --- processing --------------------------------------------------------------


class Result:
    __slots__ = ("digest", "n_packets", "n_events", "n_bytes")

    def __init__(self, digest: int, n_packets: int, n_events: int, n_bytes: int) -> None:
        self.digest = digest
        self.n_packets = n_packets
        self.n_events = n_events
        self.n_bytes = n_bytes


def process(pcm: array.array, debug: bool = False) -> Result:
    cfg = auris.Config(
        backend=auris.Backend.HOST_PCM,
        codec=auris.Codec.OPUS,
        enable_vad=True,
        vad_tier=auris.VadTier.FAST,
        vad_aggressiveness=2,
        enable_anomaly=True,
        enable_sound_class=False,
        enable_rnnoise=False,
        pcm_ring_frames=1 << 17,
        packet_queue_capacity=4096,
    )

    packets: list[auris.Packet] = []
    events: list[auris.Event] = []
    with auris.Engine(cfg) as engine:
        engine.start()
        n_frames = len(pcm) // FRAME_LEN
        for i in range(n_frames):
            engine.push_pcm(pcm[i * FRAME_LEN : (i + 1) * FRAME_LEN], i * 20_000)
            if i % 8 == 7:
                packets.extend(engine.drain_packets())
                events.extend(engine.poll_events())
        engine.stop()  # flushes the worker's tail
        packets.extend(engine.drain_packets())
        events.extend(engine.poll_events())
        stats = engine.stats()

    h = Hasher()
    h.u32(len(packets))
    n_bytes = 0
    for p in packets:
        h.u64(p.segment_id)
        h.u64(p.pts_us)
        h.u64(p.capture_ts_us)
        h.u32(p.flags)
        h.u32(int(p.codec))
        h.u32(len(p.data))
        h.bytes_(p.data)
        n_bytes += len(p.data)
    h.u32(len(events))
    for e in events:
        h.u32(int(e.type))
        h.u64(e.segment_id)
        h.u64(e.timestamp_us)
        h.u32(e.reason_mask)
        h.u32(float32_bits(e.confidence))
    h.u64(stats.frames_captured)
    h.u64(stats.packets_emitted)
    h.u64(stats.segments_opened)
    h.u64(stats.segments_closed)

    if debug:
        for i, p in enumerate(packets):
            print(
                f"P {i:02d} size={len(p.data)} flags={p.flags} seg={p.segment_id} "
                f"pts={p.pts_us} cap={p.capture_ts_us}"
            )
        for i, e in enumerate(events):
            print(
                f"E {i:02d} type={int(e.type)} reason={e.reason_mask} "
                f"conf={float32_bits(e.confidence):08x} seg={e.segment_id} ts={e.timestamp_us}"
            )
        print(
            f"S captured={stats.frames_captured} dropped={stats.frames_dropped_capture} "
            f"emitted={stats.packets_emitted} pktDropped={stats.packets_dropped} "
            f"segOpen={stats.segments_opened} segClose={stats.segments_closed}"
        )

    return Result(h.h, len(packets), len(events), n_bytes)


# --- wire protocol -----------------------------------------------------------


def write_frame(sock: socket.socket, payload: bytes) -> None:
    sock.sendall(struct.pack(">I", len(payload)) + payload)


def read_exactly(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("peer closed the connection")
        buf += chunk
    return bytes(buf)


def read_frame(sock: socket.socket) -> bytes:
    (n,) = struct.unpack(">I", read_exactly(sock, 4))
    return read_exactly(sock, n)


def pcm_to_bytes(samples: array.array) -> bytes:
    if sys.byteorder != "little":  # the wire format is little-endian int16
        samples = array.array("h", samples)
        samples.byteswap()
    return samples.tobytes()


def bytes_to_pcm(data: bytes) -> array.array:
    out = array.array("h")
    out.frombytes(data)
    if sys.byteorder != "little":
        out.byteswap()
    return out


def encode_result(computed: Result, expected: Result) -> bytes:
    return struct.pack(
        ">QQIII",
        computed.digest,
        expected.digest,
        computed.n_packets,
        computed.n_events,
        computed.n_bytes,
    )


def decode_result(raw: bytes) -> tuple[int, int]:
    computed, expected = struct.unpack(">QQ", raw[:16])
    return computed, expected


# --- peer logic --------------------------------------------------------------


def run_peer(sock: socket.socket, role: str, peer_name: str) -> bool:
    own_seed = LISTENER_SEED if role == "listen" else DIALER_SEED
    peer_seed = DIALER_SEED if role == "listen" else LISTENER_SEED
    name = "python"

    own = gen_signal(own_seed)
    expected = process(own)
    print(
        f"[{name}] generated {len(own)} samples (seed {own_seed:#x}); my engine: "
        f"{expected.n_packets} packets / {expected.n_events} events / "
        f"{expected.n_bytes} bytes, digest {expected.digest:#018x}",
        flush=True,
    )

    write_frame(sock, pcm_to_bytes(own))
    peer_bytes = read_frame(sock)

    # Generator parity: the peer's bytes must equal our own regeneration from
    # its seed — isolates a signal-generation divergence from a processing one.
    if peer_bytes != pcm_to_bytes(gen_signal(peer_seed)):
        print(
            f"[{name}] ❌ GENERATOR MISMATCH: {peer_name}'s signal differs from "
            f"our local regeneration",
            flush=True,
        )
        return False
    peer_pcm = bytes_to_pcm(peer_bytes)
    print(
        f"[{name}] received {len(peer_pcm)} samples from {peer_name}; generator "
        f"parity MATCH ✓ — processing through MY binding…",
        flush=True,
    )

    computed = process(peer_pcm)
    print(
        f"[{name}] my result for {peer_name}'s audio: {computed.n_packets} packets / "
        f"{computed.n_events} events / {computed.n_bytes} bytes, "
        f"digest {computed.digest:#018x}",
        flush=True,
    )

    write_frame(sock, encode_result(computed, expected))
    peer_computed_for_mine, peer_expected_for_theirs = decode_result(read_frame(sock))

    dir_a = peer_computed_for_mine == expected.digest  # peer processed my audio like me
    dir_b = computed.digest == peer_expected_for_theirs  # I processed theirs like them
    ok = lambda b: "MATCH ✓" if b else "MISMATCH ✗"  # noqa: E731
    print(
        f"[{name}] parity {name}-audio→{peer_name}: {ok(dir_a)}   |   "
        f"{peer_name}-audio→{name}: {ok(dir_b)}",
        flush=True,
    )
    return dir_a and dir_b


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(
            "usage:\n"
            "  conformance.py listen <port> [peer-name]\n"
            "  conformance.py dial <host:port> [peer-name]\n"
            "  conformance.py self | dump"
        )
    mode = sys.argv[1]

    if mode == "self":
        a, b = process(gen_signal(LISTENER_SEED)), process(gen_signal(LISTENER_SEED))
        match = a.digest == b.digest
        print(
            f"[self] run1 digest {a.digest:#018x}, run2 digest {b.digest:#018x} — "
            f"{'MATCH ✓' if match else 'MISMATCH ✗'}"
        )
        sys.exit(0 if match else 1)

    if mode == "dump":
        process(gen_signal(LISTENER_SEED), debug=True)
        return

    peer_name = sys.argv[3] if len(sys.argv) > 3 else "peer"
    if mode == "listen":
        port = int(sys.argv[2])
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("127.0.0.1", port))
        srv.listen(1)
        print(f"[python] listening on 127.0.0.1:{port} — start the {peer_name} peer now",
              flush=True)
        conn, _ = srv.accept()
        with conn:
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            passed = run_peer(conn, "listen", peer_name)
        srv.close()
    elif mode == "dial":
        host, _, port_s = sys.argv[2].rpartition(":")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # The listener may still be starting up; retry briefly.
        deadline = time.monotonic() + 10
        while True:
            try:
                sock.connect((host, int(port_s)))
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.2)
        with sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            passed = run_peer(sock, "dial", peer_name)
    else:
        sys.exit(f"unknown mode: {mode}")

    if passed:
        print(f"\n✅ CONFORMANCE PASS — Python and {peer_name} produce byte-identical "
              f"results both ways")
    else:
        print("\n❌ CONFORMANCE FAIL — divergence detected")
        sys.exit(1)


if __name__ == "__main__":
    main()
