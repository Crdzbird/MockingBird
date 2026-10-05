"""Which signal statistics tripped the anomaly gate."""

from enum import IntFlag


class AnomalyReason(IntFlag):
    """Bits of :attr:`Event.reason_mask` for :attr:`EventType.ANOMALY`."""

    RMS = 1
    """Loudness jumped against the rolling background."""
    SPECTRAL_FLUX = 2
    """The spectrum changed abruptly."""
    CENTROID = 4
    """The spectral centre of mass shifted."""
