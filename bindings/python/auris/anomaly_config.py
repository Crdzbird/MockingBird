"""The anomaly gate: sudden or unusual sounds against a rolling background."""

from __future__ import annotations

from dataclasses import dataclass

from auris.sensitivity import Sensitivity


@dataclass(slots=True)
class AnomalyConfig:
    """z-score triggers; lower is more sensitive."""

    enabled: bool = True
    rms_z: float = 3.0
    flux_z: float = 3.0
    background_ms: int = 2000
    """Rolling window the z-scores are measured against."""

    @classmethod
    def sensitive(cls, s: Sensitivity, *, enabled: bool = True) -> AnomalyConfig:
        """Both triggers run 5.0 → 1.5."""
        z = s.lerp(5.0, 1.5)
        return cls(enabled=enabled, rms_z=z, flux_z=z)
