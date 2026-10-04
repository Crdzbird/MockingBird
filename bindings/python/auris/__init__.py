"""Native audio capture, voice and anomaly detection, and sound classification
for Python, over the Auris C ABI.

Quick start::

    import auris

    with auris.Engine(auris.Config()) as engine:
        engine.start()
        for event in engine.events(timeout=10):
            match event:
                case auris.Event(type=auris.EventType.SPEECH_BEGIN):
                    print("voice")

One dial drives every detector::

    cfg = auris.sensitive(0.6, neural_vad_model_path="silero_vad.onnx")

The library is found automatically in a repository checkout; elsewhere set
``AURIS_LIBRARY_PATH`` or call :func:`load_library`.
"""

from auris._library import load_library, status_name, version
from auris.anomaly_config import AnomalyConfig
from auris.anomaly_reason import AnomalyReason
from auris.backend import Backend
from auris.codec import Codec
from auris.config import Config, sensitive
from auris.encoder_config import EncoderConfig
from auris.engine import Engine
from auris.error import AurisError
from auris.event import Event
from auris.event_type import EventType
from auris.packet import Packet
from auris.packet_flag import PacketFlag
from auris.performance_config import PerformanceConfig
from auris.queue_config import QueueConfig
from auris.segment_config import SegmentConfig
from auris.sensitivity import Sensitivity
from auris.sound_category import SoundCategory
from auris.sound_class_config import SoundClassConfig
from auris.stats import Stats
from auris.status import Status
from auris.vad_config import VadConfig
from auris.vad_tier import VadTier
from auris.worker_health import WorkerHealth

__all__ = [
    "AnomalyConfig",
    "AnomalyReason",
    "AurisError",
    "Backend",
    "Codec",
    "Config",
    "EncoderConfig",
    "Engine",
    "Event",
    "EventType",
    "Packet",
    "PacketFlag",
    "PerformanceConfig",
    "QueueConfig",
    "SegmentConfig",
    "Sensitivity",
    "SoundCategory",
    "SoundClassConfig",
    "Stats",
    "Status",
    "VadConfig",
    "VadTier",
    "WorkerHealth",
    "load_library",
    "sensitive",
    "status_name",
    "version",
]

__version__ = "0.1.0"
