"""Marshalling a :class:`PlayerConfig` into the native struct."""

from __future__ import annotations

from mockingbird._player_config_struct import PlayerConfigStruct
from mockingbird.player_config import PlayerConfig


def to_player_struct(cfg: PlayerConfig, defaults: PlayerConfigStruct) -> PlayerConfigStruct:
    """Fill ``defaults`` (already initialised by the library) from ``cfg``."""
    c = defaults
    c.backend = int(cfg.backend)
    c.sample_rate_hz = cfg.sample_rate_hz
    c.channels = cfg.channels
    c.target_delay_ms = cfg.target_delay_ms
    c.max_delay_ms = cfg.max_delay_ms
    c.conceal_max_ms = cfg.conceal_max_ms
    c.queue_packets = cfg.queue_packets
    return c
