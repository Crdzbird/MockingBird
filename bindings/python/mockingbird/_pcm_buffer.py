"""Coercing Python audio into a ctypes int16 buffer."""

from __future__ import annotations

import ctypes
from typing import Any


def as_int16_buffer(samples: Any) -> tuple[Any, int]:
    """Accepts bytes, ``array('h')``, numpy int16 arrays, memoryviews, or a
    plain sequence of ints. Returns ``(pointer, sample_count)``."""
    try:
        view = memoryview(samples)
    except TypeError:
        seq = list(samples)
        return (ctypes.c_int16 * len(seq))(*seq), len(seq)
    if view.ndim != 1 or view.format not in ("h", "B", "b"):
        view = view.cast("B").cast("h")
    elif view.format != "h":
        view = view.cast("h")
    count = view.shape[0]
    raw = (ctypes.c_char * (count * 2)).from_buffer_copy(view.tobytes())
    return ctypes.cast(raw, ctypes.POINTER(ctypes.c_int16)), count
