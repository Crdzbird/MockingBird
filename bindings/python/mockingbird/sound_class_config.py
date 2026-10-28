"""YAMNet sound classification over all 521 AudioSet classes."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird.sensitivity import Sensitivity


@dataclass(slots=True)
class SoundClassConfig:
    """Emits ``SOUND_CLASS`` events when the top-1 probability clears
    ``threshold``."""

    enabled: bool = False
    model_path: str | None = None
    """Path to yamnet_3s.onnx."""
    threshold: float = 0.4
    interval_ms: int = 1000
    """Inference cadence; halving it roughly halves the cost."""

    @classmethod
    def sensitive(
        cls,
        s: Sensitivity,
        *,
        enabled: bool = True,
        model_path: str | None = None,
        interval_ms: int = 1000,
    ) -> SoundClassConfig:
        """The threshold runs 0.70 to 0.12."""
        return cls(
            enabled=enabled,
            model_path=model_path,
            threshold=s.lerp(0.70, 0.12),
            interval_ms=interval_ms,
        )
