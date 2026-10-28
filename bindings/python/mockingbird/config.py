"""Everything an :class:`Engine` needs, grouped by concern."""

from __future__ import annotations

from dataclasses import dataclass, field

from mockingbird.anomaly_config import AnomalyConfig
from mockingbird.backend import Backend
from mockingbird.encoder_config import EncoderConfig
from mockingbird.performance_config import PerformanceConfig
from mockingbird.queue_config import QueueConfig
from mockingbird.segment_config import SegmentConfig
from mockingbird.sensitivity import Sensitivity
from mockingbird.sound_class_config import SoundClassConfig
from mockingbird.vad_config import VadConfig
from mockingbird.vad_tier import VadTier
from mockingbird.voice_processing import VoiceProcessing


@dataclass(slots=True)
class Config:
    """The defaults give fast VAD and the anomaly gate on the platform's
    microphone at 16 kHz."""

    backend: Backend = Backend.AUTO
    sample_rate_hz: int = 16000
    frame_ms: int = 20
    denoise: bool = False
    """RNNoise ahead of every detector; needs 8, 16 or 48 kHz."""
    voice_processing: VoiceProcessing = VoiceProcessing.NONE
    """The platform's echo cancellation, gain control and noise suppression."""
    vad: VadConfig = field(default_factory=VadConfig)
    anomaly: AnomalyConfig = field(default_factory=AnomalyConfig)
    sound_class: SoundClassConfig = field(default_factory=SoundClassConfig)
    segments: SegmentConfig = field(default_factory=SegmentConfig)
    encoder: EncoderConfig = field(default_factory=EncoderConfig)
    queues: QueueConfig = field(default_factory=QueueConfig)
    performance: PerformanceConfig = field(default_factory=PerformanceConfig)


def sensitive(
    dial: float,
    *,
    backend: Backend = Backend.AUTO,
    denoise: bool = False,
    vad: bool = True,
    vad_tier: VadTier = VadTier.NEURAL,
    neural_vad_model_path: str | None = None,
    anomaly: bool = True,
    sound_class: bool = True,
    sound_class_model_path: str | None = None,
    encoder: EncoderConfig | None = None,
    queues: QueueConfig | None = None,
    performance: PerformanceConfig | None = None,
) -> Config:
    """Every detector on, driven by one 0..1 ``dial``, with neural VAD so
    voice detection is multilingual."""
    s = Sensitivity(dial)
    return Config(
        backend=backend,
        denoise=denoise,
        vad=VadConfig.sensitive(s, enabled=vad, tier=vad_tier, model_path=neural_vad_model_path),
        anomaly=AnomalyConfig.sensitive(s, enabled=anomaly),
        sound_class=SoundClassConfig.sensitive(
            s, enabled=sound_class, model_path=sound_class_model_path
        ),
        encoder=encoder or EncoderConfig(),
        queues=queues or QueueConfig(),
        performance=performance or PerformanceConfig(),
    )
