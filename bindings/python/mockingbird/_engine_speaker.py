"""Speaker verification: the model, voice prints and the target speaker."""

from __future__ import annotations

import ctypes
import os
from collections.abc import Sequence
from typing import Any

from mockingbird._engine_handle import EngineHandle
from mockingbird._pcm_buffer import as_int16_buffer


class EngineSpeaker(EngineHandle):
    """Tell whether each utterance comes from one enrolled person."""

    def set_speaker_model(self, model: str | os.PathLike[str] | None) -> None:
        """Name the speaker embedding model; ``None`` falls back to
        ``MOCKINGBIRD_SPEAKER_MODEL``, then ``speaker_campplus.onnx``. Only
        while stopped."""
        self._call("mockingbird_set_speaker_model", os.fsencode(model) if model else None)

    def speaker_embedding(self, samples: Any) -> list[float]:
        """The voice print of 16 kHz int16 mono samples, at least 0.5 s;
        3 to 10 s of one person speaking works best."""
        buf, count = as_int16_buffer(samples)
        out = (ctypes.c_float * 4096)()
        dim = ctypes.c_uint32()
        self._call("mockingbird_speaker_embedding", buf, count, out, len(out), ctypes.byref(dim))
        return list(out[: dim.value])

    def set_target_speaker(self, embedding: Sequence[float] | None, threshold: float = 0.0) -> None:
        """Compare every utterance with ``embedding`` and report a ``SPEAKER``
        event; ``threshold`` is the cosine similarity that counts as a match,
        0 for the default. ``None`` stops."""
        if embedding is None:
            self._call("mockingbird_set_target_speaker", None, 0, threshold)
            return
        arr = (ctypes.c_float * len(embedding))(*embedding)
        self._call("mockingbird_set_target_speaker", arr, len(embedding), threshold)
