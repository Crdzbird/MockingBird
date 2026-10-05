"""Knobs that trade latency for CPU and battery."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class PerformanceConfig:
    """Every field is live-tunable through :meth:`Engine.update_config`."""

    silence_gate_dbfs: float = -60.0
    """Frames below this level skip RNNoise, and YAMNet skips an inference
    whose whole window was quiet. -120 or lower disables."""
    adaptive_degradation: bool = False
    """Shed YAMNet work under load and restore it when calm. Off by default
    because it reacts to wall-clock time, which offline runs must not."""
    degrade_busy_ppm: int = 700_000
    """Worker busy share, parts per million, that triggers degradation."""
    worker_batch_frames: int = 0
    """Wake the worker only once this many frames are queued: the same CPU
    work in fewer wakeups, for a little latency."""

    @classmethod
    def mobile(cls) -> PerformanceConfig:
        """Degradation on and four frames per wakeup, for a live microphone."""
        return cls(adaptive_degradation=True, worker_batch_frames=4)
