"""Which voice-activity detector runs. Both are language-agnostic."""

from enum import IntEnum


class VadTier(IntEnum):
    """Mirrors ``auris_vad_tier_t``."""

    FAST = 0
    """libfvad: zero-dependency and cheap."""
    NEURAL = 1
    """Silero v5 via ONNX Runtime."""
