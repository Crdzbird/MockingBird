"""One thing the engine observed."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird._event_struct import EventStruct
from mockingbird.anomaly_reason import AnomalyReason
from mockingbird.event_type import EventType
from mockingbird.sound_category import SoundCategory
from mockingbird.status import Status


@dataclass(frozen=True, slots=True)
class Event:
    """The typed properties interpret ``reason_mask`` and ``confidence`` for
    the event types that carry a payload. Match on ``type``::

        match event:
            case Event(type=EventType.SOUND_CLASS, category=c):
                ...
    """

    type: EventType | int
    segment_id: int
    timestamp_us: int
    reason_mask: int
    confidence: float

    @classmethod
    def from_struct(cls, e: EventStruct) -> Event:
        """Read a native ``mockingbird_event_t``; unknown types keep the raw int."""
        kind = EventType(e.type) if e.type in EventType._value2member_map_ else e.type
        return cls(kind, e.segment_id, e.timestamp_us, e.reason_mask, e.confidence)

    @property
    def sound_class(self) -> int:
        """The AudioSet class of a ``SOUND_CLASS`` event."""
        return self.reason_mask

    @property
    def category(self) -> SoundCategory:
        """The broad category of a ``SOUND_CLASS`` event."""
        return SoundCategory.of(self.sound_class)

    @property
    def is_human_voice(self) -> bool:
        """True if a ``SOUND_CLASS`` event heard a person's voice."""
        return SoundCategory.is_human_voice(self.sound_class)

    @property
    def anomaly_reasons(self) -> AnomalyReason:
        """The statistics that tripped an ``ANOMALY`` event."""
        return AnomalyReason(self.reason_mask)

    @property
    def degradation_level(self) -> int:
        """The new level of a ``DEGRADATION`` event, 0..3."""
        return self.reason_mask

    @property
    def busy_fraction(self) -> float:
        """The worker busy share that triggered a ``DEGRADATION`` event."""
        return self.confidence

    @property
    def backend_error(self) -> Status | int:
        """The status of a ``BACKEND_ERROR`` event, stored as a uint32."""
        signed = self.reason_mask - (1 << 32) if self.reason_mask >= 1 << 31 else self.reason_mask
        return Status.of(signed)
