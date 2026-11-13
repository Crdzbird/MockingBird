"""A snapshot of a player's counters."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird._player_stats_struct import PlayerStatsStruct
from mockingbird.player_state import PlayerState


@dataclass(frozen=True, slots=True)
class PlayerStats:
    """Never carries audio."""

    packets_received: int = 0
    """Accepted by :meth:`Player.push`."""
    packets_played: int = 0
    packets_concealed: int = 0
    """Missing packets replaced by concealment."""
    packets_dropped: int = 0
    """Late, duplicate, over ``max_delay_ms``, no free slot, or undecodable."""
    packets_late: int = 0
    """The late and duplicate share of ``packets_dropped``."""
    underruns: int = 0
    frames_rendered: int = 0
    """Frames given to the output, silence too."""
    buffered_packets: int = 0
    buffered_ms: int = 0
    state: PlayerState | int = PlayerState.STOPPED
    output_restarts: int = 0
    """Route changes the output followed."""

    @classmethod
    def from_struct(cls, s: PlayerStatsStruct) -> PlayerStats:
        """Read a native ``mockingbird_player_stats_t``."""
        state = PlayerState(s.state) if s.state in PlayerState._value2member_map_ else s.state
        return cls(
            s.packets_received,
            s.packets_played,
            s.packets_concealed,
            s.packets_dropped,
            s.packets_late,
            s.underruns,
            s.frames_rendered,
            s.buffered_packets,
            s.buffered_ms,
            state,
            s.output_restarts,
        )
