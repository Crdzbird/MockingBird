"""WAV reading and the frame grid every evaluation shares."""

import wave
from pathlib import Path

import numpy as np

RATE = 16000
FRAME = 320
"""20 ms, the engine's frame and the scoring grid."""


def read(path: Path) -> np.ndarray:
    with wave.open(str(path)) as w:
        assert w.getframerate() == RATE and w.getnchannels() == 1, path
        return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32) / 32768.0


def frames(signal: np.ndarray) -> int:
    return len(signal) // FRAME


def rms_db(signal: np.ndarray) -> np.ndarray:
    n = frames(signal)
    x = signal[: n * FRAME].reshape(n, FRAME)
    return 10 * np.log10(np.mean(x * x, axis=1) + 1e-12)
