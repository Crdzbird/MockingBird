"""Decoding Opus packets, the engine's or any encoder's, back into int16 PCM."""

from __future__ import annotations

import ctypes
from array import array
from typing import Any

from mockingbird._byte_buffer import as_byte_buffer
from mockingbird._library import check, library
from mockingbird._pcm_array import pcm_array
from mockingbird._playback_signatures import DecoderPtr
from mockingbird.error import MockingbirdError
from mockingbird.status import Status


class Decoder:
    """One stream's decoder: :meth:`decode` its packets in order and
    :meth:`conceal` each one that never arrived. Output is ``frames *
    channels`` interleaved int16 values. Drive it from one thread."""

    def __init__(self, sample_rate_hz: int = 16000, channels: int = 1) -> None:
        self._lib = library()
        self._handle = DecoderPtr()
        self._closed = True
        self._sample_rate_hz = sample_rate_hz
        self._channels = channels
        self._max_frames = sample_rate_hz // 1000 * 120
        code = self._lib.mockingbird_decoder_create(sample_rate_hz, channels, ctypes.byref(self._handle))
        check(code, "mockingbird_decoder_create")
        self._closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    @property
    def sample_rate_hz(self) -> int:
        """The output rate."""
        return self._sample_rate_hz

    @property
    def channels(self) -> int:
        """1 or 2; a stereo packet is mixed down for a mono decoder."""
        return self._channels

    def decode(self, packet: Any) -> array:
        """Decode one packet of 1..65536 bytes from any bytes-like object.
        A packet libopus cannot parse raises ``ERR_INVALID_ARG`` and leaves
        the decoder usable."""
        data, size = as_byte_buffer(packet)
        out, frames = self._buffer(), ctypes.c_uint32(0)
        self._call("mockingbird_decoder_decode", data, size, out, self._max_frames, ctypes.byref(frames))
        return pcm_array(out, frames.value * self._channels)

    def conceal(self, frames: int = 0) -> array:
        """Synthesise ``frames`` frames for one missing packet, a multiple of
        2.5 ms up to 120 ms; 0 repeats the last packet's duration, or 20 ms
        before the first one."""
        out, written = self._buffer(), ctypes.c_uint32(0)
        self._call("mockingbird_decoder_conceal", frames, out, self._max_frames, ctypes.byref(written))
        return pcm_array(out, written.value * self._channels)

    def reset(self) -> None:
        """Forget the history, for a new stream on the same decoder."""
        self._call("mockingbird_decoder_reset")

    def close(self) -> None:
        """Destroy the native decoder. Idempotent."""
        if getattr(self, "_closed", True):
            return
        self._closed = True
        if self._handle:
            self._lib.mockingbird_decoder_destroy(self._handle)
            self._handle = DecoderPtr()

    def _buffer(self) -> ctypes.Array:
        return (ctypes.c_int16 * (self._max_frames * self._channels))()

    def _call(self, name: str, *args: object) -> None:
        if self._closed:
            raise MockingbirdError("decoder", Status.ERR_INVALID_STATE, "decoder is closed")
        check(getattr(self._lib, name)(self._handle, *args), name)
