"""How detections are cut into segments of audio."""

from dataclasses import dataclass


@dataclass(slots=True)
class SegmentConfig:
    """All values in milliseconds."""

    pre_roll_ms: int = 300
    """Audio kept from before the trigger."""
    post_roll_ms: int = 300
    """Audio kept after the detectors go quiet; the segment closes after the
    longer of this and the VAD hangover."""
    max_ms: int = 60000
    """Hard cap: the packet that reaches it is the last, and the next active
    frame opens a new segment. 0 is unbounded."""
