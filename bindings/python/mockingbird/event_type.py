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
