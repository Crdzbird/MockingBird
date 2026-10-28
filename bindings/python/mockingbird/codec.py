"""How packets are encoded."""

from enum import IntEnum


class Codec(IntEnum):
    """Mirrors ``mockingbird_codec_t``."""

    PCM_S16LE = 0
    OPUS = 1
    FLAC = 2
    """Declared by the ABI, not implemented yet."""
    OGG_OPUS = 3
    """Opus in Ogg: one segment's payloads, concatenated, form a .opus file."""
