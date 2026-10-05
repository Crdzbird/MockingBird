"""Shared fixtures for the Python binding tests."""

from __future__ import annotations

import array
import sys
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import mockingbird

FIXTURES = Path(__file__).resolve().parents[3] / "tests" / "fixtures"
FRAME = 320


def host_config(**overrides: object) -> mockingbird.Config:
    """A HOST_PCM, PCM-passthrough config with every detector off unless given."""
    cfg = mockingbird.Config(backend=mockingbird.Backend.HOST_PCM)
    cfg.vad.enabled = False
    cfg.anomaly.enabled = False
    for key, value in overrides.items():
        setattr(cfg, key, value)
    return cfg


def silence(n: int = FRAME) -> array.array:
    return array.array("h", [0] * n)


def square(n: int, amp: int, period: int = 2) -> array.array:
    half = period // 2
    return array.array("h", [amp if i % period < half else -amp for i in range(n)])


def read_wav(path: Path) -> tuple[array.array, int]:
    with wave.open(str(path)) as w:
        pcm = array.array("h")
        pcm.frombytes(w.readframes(w.getnframes()))
        return pcm, w.getframerate()


def push(engine: mockingbird.Engine, frame: array.array, count: int, ts: int = 0) -> int:
    """Push ``count`` copies of ``frame`` 20 ms apart; returns the next timestamp."""
    for _ in range(count):
        engine.push_pcm(frame, ts)
        ts += 20_000
    return ts
