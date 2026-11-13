"""What a player is doing right now."""

from enum import IntEnum


class PlayerState(IntEnum):
    """Mirrors ``mockingbird_player_state_t``."""

    STOPPED = 0
    IDLE = 1
    """Started, nothing to play."""
    BUFFERING = 2
    """Waiting for ``target_delay_ms`` of audio."""
    PLAYING = 3
    PAUSED = 4
    """By :meth:`Player.pause`, or by the route going away."""
    INTERRUPTED = 5
    """The OS took the output away."""
