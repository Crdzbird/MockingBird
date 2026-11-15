"""How the platform runs live capture."""

from enum import IntEnum


class CapturePerformance(IntEnum):
    """Values of ``mockingbird_config_t.capture_performance``. Only Android
    (Oboe) and the miniaudio backend act on it; Core Audio and host PCM ignore
    it. Not live-tunable: :meth:`Engine.update_config` refuses a change."""

    DEFAULT = 0
    """The platform's normal capture path. On Android it is Oboe's
    PerformanceMode::None, which keeps real time while a camera pipeline
    shares the audio DSP."""
    LOW_LATENCY = 1
    """The fast path (Oboe LowLatency, AAudio MMAP), which can stall and drop
    audio when another heavy client shares it."""
    POWER_SAVING = 2
    """Large buffers (Oboe PowerSaving, miniaudio's conservative profile)."""
