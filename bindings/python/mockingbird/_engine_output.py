"""Draining events, packets, the PCM tap and stats."""

from __future__ import annotations

import ctypes
import time
from collections.abc import Iterator

from mockingbird._engine_handle import EngineHandle
from mockingbird._event_struct import EventStruct
from mockingbird._library import check
from mockingbird._packet_struct import PacketStruct
from mockingbird._stats_struct import StatsStruct
from mockingbird.event import Event
from mockingbird.packet import Packet
from mockingbird.stats import Stats
from mockingbird.status import Status


class EngineOutput(EngineHandle):
    """Mixed into :class:`Engine`."""

    def poll_event(self) -> Event | None:
        """One queued event, or ``None``."""
        self._require_open()
        ev = EventStruct()
        code = self._lib.mockingbird_poll_event(self._handle, ctypes.byref(ev))
        if code == Status.ERR_QUEUE_EMPTY:
            return None
        check(code, "mockingbird_poll_event")
        return Event.from_struct(ev)

    def poll_events(self) -> list[Event]:
        """Every queued event."""
        return list(iter(self.poll_event, None))

    def drain_packet(self) -> Packet | None:
        """One queued packet with its bytes copied, or ``None``."""
        self._require_open()
        pkt = PacketStruct()
        code = self._lib.mockingbird_drain(self._handle, ctypes.byref(pkt))
        if code == Status.ERR_QUEUE_EMPTY:
            return None
        check(code, "mockingbird_drain")
        out = Packet.from_struct(pkt)
        self._call("mockingbird_release", ctypes.byref(pkt))
        return out

    def drain_packets(self) -> list[Packet]:
        """Every queued packet."""
        return list(iter(self.drain_packet, None))

    def read_pcm(self, max_samples: int = 4096) -> bytes:
        """Up to ``max_samples`` from the ungated PCM tap as little-endian
        int16 bytes; needs ``queues.pcm_tap``."""
        if max_samples <= 0:
            return b""
        buf = (ctypes.c_int16 * max_samples)()
        written = ctypes.c_uint32(0)
        self._call("mockingbird_read_pcm", buf, max_samples, ctypes.byref(written))
        return bytes(memoryview(buf).cast("B")[: written.value * 2])

    def events(self, timeout: float | None = None, poll_interval: float = 0.02) -> Iterator[Event]:
        """Yield events as they arrive until ``timeout`` seconds pass or the
        engine is closed."""
        deadline = None if timeout is None else time.monotonic() + timeout
        while not self._closed:
            yield from self.poll_events()
            if deadline is not None and time.monotonic() >= deadline:
                return
            time.sleep(poll_interval)

    def stats(self) -> Stats:
        """A snapshot of the engine's counters."""
        st = StatsStruct()
        self._call("mockingbird_get_stats", ctypes.byref(st))
        return Stats.from_struct(st)
