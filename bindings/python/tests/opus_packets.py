"""Opus packets made by the engine itself, for the decoder and player tests."""

from __future__ import annotations

import array
import math
import time

from helpers import FRAME, host_config, mockingbird, push

RATE = 16000
PACKETS = 15


def tone(n: int = FRAME, freq: float = 440.0, amp: int = 8000) -> array.array:
    return array.array("h", [int(amp * math.sin(2 * math.pi * freq * i / RATE)) for i in range(n)])


def opus_packets(count: int = PACKETS) -> list[mockingbird.Packet]:
    """``count`` 20 ms Opus packets of a 440 Hz tone from a HOST_PCM engine."""
    cfg = host_config(encoder=mockingbird.EncoderConfig.opus())
    packets: list[mockingbird.Packet] = []
    with mockingbird.Engine(cfg) as e:
        e.start()
        push(e, tone(FRAME * count), 1)
        deadline = time.monotonic() + 2.0
        while len(packets) < count and time.monotonic() < deadline:
            packets += e.drain_packets()
            time.sleep(0.02)
    return packets


def host_player(**overrides: object) -> mockingbird.Player:
    """A started HOST_PCM player with room for every test packet."""
    cfg = mockingbird.PlayerConfig(
        backend=mockingbird.Backend.HOST_PCM, max_delay_ms=10000, queue_packets=256
    )
    for key, value in overrides.items():
        setattr(cfg, key, value)
    player = mockingbird.Player(cfg)
    player.start()
    return player


def read_all(player: mockingbird.Player, frames: int) -> array.array:
    """Pull ``frames`` frames in 10 ms steps, the way an audio callback would."""
    out = array.array("h")
    for _ in range(0, frames, 160):
        out += player.read(160)
    return out
