"""Result codes of the native engine."""

from enum import IntEnum


class Status(IntEnum):
    """Mirrors ``auris_status_t``."""

    OK = 0
    ERR_INVALID_ARG = -1
    ERR_INVALID_STATE = -2
    ERR_NO_MEMORY = -3
    ERR_BACKEND = -4
    ERR_NOT_RUNNING = -5
    ERR_ALREADY_RUNNING = -6
    ERR_QUEUE_EMPTY = -7
    ERR_PERMISSION = -8
    ERR_UNSUPPORTED = -9
    ERR_INTERNAL = -10

    @classmethod
    def of(cls, value: int) -> "Status | int":
        """The status for ``value``, or the raw int when the code is unknown."""
        try:
            return cls(value)
        except ValueError:
            return value
