"""What an :class:`Event` reports."""

from enum import IntEnum


class EventType(IntEnum):
    """Mirrors ``mockingbird_event_type_t``."""

    SPEECH_BEGIN = 1
    SPEECH_END = 2
    ANOMALY = 3
    SEGMENT_BEGIN = 4
    SEGMENT_END = 5
    OVERRUN = 6
    BACKEND_ERROR = 7
    SOUND_CLASS = 8
    DEGRADATION = 9
    CAPTURE_INTERRUPTED = 10
    """The OS took the microphone away until CAPTURE_RESUMED."""
    CAPTURE_RESUMED = 11
    ROUTE_CHANGED = 12
    """The input device changed; capture continues on the new route."""
    KEYWORD = 13
    """A keyword was heard: ``reason_mask`` is its id, ``confidence`` its score."""
