"""Coercing a bytes-like packet into a ctypes byte buffer."""

from __future__ import annotations

import ctypes
from typing import Any


def as_byte_buffer(data: Any) -> tuple[Any, int]:
    """Accepts bytes, bytearray, ``array('h')``, numpy arrays, memoryviews or
    any other buffer-protocol object. Returns ``(buffer, byte_count)``."""
    raw = memoryview(data).tobytes()
    return (ctypes.c_uint8 * len(raw)).from_buffer_copy(raw), len(raw)
