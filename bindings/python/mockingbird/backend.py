"""Where the engine takes audio from."""

from enum import IntEnum


class Backend(IntEnum):
    """Mirrors ``mockingbird_backend_t``."""

    AUTO = 0
    MINIAUDIO = 1
    OBOE = 2
    COREAUDIO = 3
    HOST_PCM = 4
    """No microphone: the host feeds :meth:`Engine.push_pcm`."""
