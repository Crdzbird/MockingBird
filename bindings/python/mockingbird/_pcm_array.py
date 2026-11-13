"""Copying native int16 output into a Python ``array('h')``."""

from __future__ import annotations

from array import array
from typing import Any


def pcm_array(buf: Any, count: int) -> array:
    """The first ``count`` int16 values of the ctypes buffer ``buf``."""
    out = array("h")
    out.frombytes(memoryview(buf).cast("B")[: count * 2])
    return out
