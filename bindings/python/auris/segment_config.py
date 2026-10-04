"""How detections are cut into segments of audio."""

from dataclasses import dataclass


@dataclass(slots=True)
class SegmentConfig:
    """All values in milliseconds."""

    pre_roll_ms: int = 300
    """Audio kept from before the trigger."""
    post_roll_ms: int = 300
    """Audio kept after the detector closes."""
    max_ms: int = 60000
    """Hard cap on one segment; 0 is unbounded."""
