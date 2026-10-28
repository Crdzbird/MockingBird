"""The platform's own voice processing for live capture."""

from enum import IntFlag


class VoiceProcessing(IntFlag):
    """Bits of ``mockingbird_config_t.voice_processing``. Each platform applies
    what it has; :attr:`Stats.capture` reports what is in effect."""

    NONE = 0
    ECHO_CANCEL = 1
    """Remove the device's own playback from the input."""
    GAIN_CONTROL = 2
    """Level the input automatically."""
    NOISE_SUPPRESS = 4
    """The platform's noise suppressor, separate from RNNoise."""
    ALL = 7
