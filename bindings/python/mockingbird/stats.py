"""A snapshot of the engine's counters."""

from __future__ import annotations

from dataclasses import dataclass, field

from mockingbird._stats_struct import StatsStruct

from mockingbird.capture_health import CaptureHealth
from mockingbird.worker_health import WorkerHealth


@dataclass(frozen=True, slots=True)
class Stats:
    """Never carries audio."""

    frames_captured: int = 0
    frames_dropped_capture: int = 0
    packets_emitted: int = 0
    packets_dropped: int = 0
    segments_opened: int = 0
    segments_closed: int = 0
    pcm_ring_fill_frames: int = 0
    packet_queue_fill: int = 0
    input_rms_dbfs: float = 0.0
    """Latest frame's level before denoising; -120 is digital silence."""
    input_peak_dbfs: float = 0.0
    tap_samples_dropped: int = 0
    """PCM-tap samples the host did not drain in time."""
    worker: WorkerHealth = field(default_factory=WorkerHealth)
    capture: CaptureHealth = field(default_factory=CaptureHealth)

    @classmethod
    def from_struct(cls, s: StatsStruct) -> Stats:
        """Read a native ``mockingbird_stats_t``."""
        return cls(
            s.frames_captured,
            s.frames_dropped_capture,
            s.packets_emitted,
            s.packets_dropped,
            s.segments_opened,
            s.segments_closed,
            s.pcm_ring_fill_frames,
            s.packet_queue_fill,
            s.input_rms_dbfs,
            s.input_peak_dbfs,
            s.tap_samples_dropped,
            WorkerHealth.from_struct(s),
            CaptureHealth.from_struct(s),
        )
