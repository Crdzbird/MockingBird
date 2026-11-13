"""Everything a :class:`Player` needs: output and jitter buffer settings."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird.backend import Backend


@dataclass(slots=True)
class PlayerConfig:
    """The defaults match ``mockingbird_player_config_defaults``: the
    platform's output at 16 kHz mono."""

    backend: Backend = Backend.AUTO
    """A device backend, or ``HOST_PCM`` to pull audio with :meth:`Player.read`."""
    sample_rate_hz: int = 16000
    """8000, 12000, 16000, 24000 or 48000."""
    channels: int = 1
    """1 or 2."""
    target_delay_ms: int = 60
    """0..2000; how much audio the first packet waits for."""
    max_delay_ms: int = 400
    """target..10000; more buffered drops the oldest packets."""
    conceal_max_ms: int = 100
    """0..1000; longer holes are silence between talkspurts."""
    queue_packets: int = 128
    """Packet slots, 4..4096."""
