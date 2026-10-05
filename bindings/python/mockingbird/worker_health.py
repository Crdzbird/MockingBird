"""How hard the native worker thread is working and what it is shedding."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird._stats_struct import StatsStruct


@dataclass(frozen=True, slots=True)
class WorkerHealth:
    """The worker fields of a :class:`Stats` snapshot."""

    busy_ppm: int = 0
    """Share of wall time spent processing over the last ~1 s, parts per
    million; 1_000_000 is saturated."""
    max_frame_us: int = 0
    """Longest single-frame processing time since start."""
    wakeups: int = 0
    """Thread parks and unparks, a battery proxy on phones."""
    sound_class_inferences: int = 0
    sound_class_inferences_skipped: int = 0
    """Inferences shed by the silence gate or degradation."""
    preproc_frames_skipped: int = 0
    """Frames RNNoise skipped as quiet."""
    degradation_level: int = 0
    """0 nominal through 3 classifier suspended."""

    @classmethod
    def from_struct(cls, s: StatsStruct) -> WorkerHealth:
        """Read the worker fields of a native ``mockingbird_stats_t``."""
        return cls(
            s.worker_busy_ppm,
            s.worker_max_frame_us,
            s.worker_wakeups,
            s.sound_class_inferences,
            s.sound_class_inferences_skipped,
            s.preproc_frames_skipped,
            s.degradation_level,
        )

    @property
    def busy_fraction(self) -> float:
        """:attr:`busy_ppm` as 0..1."""
        return self.busy_ppm / 1_000_000
