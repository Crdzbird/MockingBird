"""What the capture backend is doing for the microphone."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird._stats_struct import StatsStruct
from mockingbird.voice_processing import VoiceProcessing


@dataclass(frozen=True, slots=True)
class CaptureHealth:
    """Voice processing in effect and capture restarts."""

    voice_processing: VoiceProcessing = VoiceProcessing.NONE
    """What the platform applied, which can be less than asked for."""
    restarts: int = 0
    """Returns after an interruption or a route change."""

    @classmethod
    def from_struct(cls, s: StatsStruct) -> CaptureHealth:
        """Read the capture fields of a native ``mockingbird_stats_t``."""
        return cls(VoiceProcessing(s.voice_processing_active), s.capture_restarts)
