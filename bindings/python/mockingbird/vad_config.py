"""Voice-activity detection: the language-agnostic speech events."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird.sensitivity import Sensitivity
from mockingbird.vad_tier import VadTier


@dataclass(slots=True)
class VadConfig:
    """Thresholds for :attr:`VadTier.FAST` and :attr:`VadTier.NEURAL`."""

    enabled: bool = True
    tier: VadTier = VadTier.FAST
    aggressiveness: int = 2
    """libfvad 0..3; higher rejects borderline frames."""
    neural_threshold: float = 0.5
    """Silero P(speech); lower is more sensitive."""
    hangover_ms: int = 300
    """Trailing silence before a speech segment closes."""
    model_path: str | None = None
    """Path to silero_vad.onnx for the neural tier."""

    @classmethod
    def sensitive(
        cls,
        s: Sensitivity,
        *,
        enabled: bool = True,
        tier: VadTier = VadTier.NEURAL,
        model_path: str | None = None,
    ) -> VadConfig:
        """Silero's threshold runs 0.80 to 0.20, libfvad's aggressiveness 3 to 0."""
        return cls(
            enabled=enabled,
            tier=tier,
            aggressiveness=round(s.lerp(3, 0)),
            neural_threshold=s.lerp(0.80, 0.20),
            model_path=model_path,
        )
