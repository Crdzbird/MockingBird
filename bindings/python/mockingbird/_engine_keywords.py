"""Keyword spotting: the shared front end and the keyword models."""

from __future__ import annotations

import ctypes
import os

from mockingbird._engine_handle import EngineHandle


def _path(p: str | os.PathLike[str] | None) -> bytes | None:
    return os.fsencode(p) if p else None


class EngineKeywords(EngineHandle):
    """Listen for openWakeWord-compatible keywords. Only while stopped; changes
    apply from the next :meth:`start`."""

    def set_keyword_frontend(
        self, melspectrogram: str | os.PathLike[str] | None, embedding: str | os.PathLike[str] | None
    ) -> None:
        """Name the mel spectrogram and speech embedding models every keyword
        shares. ``None`` falls back to the ``MOCKINGBIRD_KEYWORD_*`` environment
        variables, then to the default file names in the working directory."""
        self._call("mockingbird_set_keyword_frontend", _path(melspectrogram), _path(embedding))

    def add_keyword(self, model: str | os.PathLike[str], threshold: float = 0.5) -> int:
        """Register a keyword model and return the id its ``KEYWORD`` events
        carry in ``reason_mask``. Up to 16 keywords."""
        out = ctypes.c_uint32()
        self._call("mockingbird_add_keyword", os.fsencode(model), threshold, ctypes.byref(out))
        return out.value

    def clear_keywords(self) -> None:
        """Remove every keyword."""
        self._call("mockingbird_clear_keywords")
