"""Playing Opus or PCM packets through a jitter buffer on the device output."""

from __future__ import annotations

import ctypes
from array import array
from typing import Any

from mockingbird._byte_buffer import as_byte_buffer
from mockingbird._library import check
from mockingbird._pcm_array import pcm_array
from mockingbird._player_config_struct import PlayerConfigStruct
from mockingbird._player_handle import PlayerHandle
from mockingbird._player_marshal import to_player_struct
from mockingbird._player_stats_struct import PlayerStatsStruct
from mockingbird.codec import Codec
from mockingbird.packet import Packet
from mockingbird.player_config import PlayerConfig
from mockingbird.player_stats import PlayerStats


class Player(PlayerHandle):
    """Create it, :meth:`push` packets ordered by ``pts_us``, :meth:`start`
    it, and :meth:`close` it; a ``with`` block does the last part for you.
    With ``Backend.HOST_PCM`` no device opens and :meth:`read` pulls the
    output instead."""

    def __init__(self, config: PlayerConfig | None = None) -> None:
        self._bind()
        self._config = config or PlayerConfig()
        defaults = PlayerConfigStruct()
        check(self._lib.mockingbird_player_config_defaults(ctypes.byref(defaults)), "mockingbird_player_config_defaults")
        c = to_player_struct(self._config, defaults)
        check(self._lib.mockingbird_player_create(ctypes.byref(c), ctypes.byref(self._handle)), "mockingbird_player_create")

    @property
    def config(self) -> PlayerConfig:
        """The configuration the player was created with."""
        return self._config

    def push(self, data: Any, pts_us: int, codec: Codec = Codec.OPUS, flags: int = 0) -> None:
        """Queue one packet of 1..8192 bytes and at most 120 ms: Opus bytes,
        or int16 PCM at the player's rate and channel count. ``flags`` are
        :class:`PacketFlag` bits; ``LAST`` ends a talkspurt. The bytes are
        copied; a packet with no free slot is dropped and counted."""
        buf, size = as_byte_buffer(data)
        self._call("mockingbird_player_push", int(codec), buf, size, pts_us, int(flags))

    def push_packet(self, packet: Packet) -> None:
        """Queue a :class:`Packet` drained from an engine with its codec and
        flags, at its ``capture_ts_us``: one clock for the whole run, where
        ``pts_us`` restarts with every segment."""
        self.push(packet.data, packet.capture_ts_us, packet.codec, packet.flags)

    def flush(self) -> None:
        """Drop everything pushed so far and start a new timeline."""
        self._call("mockingbird_player_flush")

    def read(self, frames: int) -> array:
        """``HOST_PCM`` only: render exactly ``frames`` frames, at most one
        second, as ``frames * channels`` interleaved int16 values, silence
        where there is nothing to play."""
        if frames <= 0:
            return array("h")
        count = frames * self._config.channels
        buf = (ctypes.c_int16 * count)()
        self._call("mockingbird_player_read", buf, frames)
        return pcm_array(buf, count)

    def set_volume(self, volume: float) -> None:
        """Linear gain in [0, 1], applied with a short ramp."""
        self._call("mockingbird_player_set_volume", float(volume))

    def set_muted(self, muted: bool) -> None:
        """Silence the output without losing the volume."""
        self._call("mockingbird_player_set_muted", int(bool(muted)))

    def stats(self) -> PlayerStats:
        """A snapshot of the player's counters and state."""
        st = PlayerStatsStruct()
        self._call("mockingbird_player_get_stats", ctypes.byref(st))
        return PlayerStats.from_struct(st)
