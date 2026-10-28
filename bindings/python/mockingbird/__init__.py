"""Native audio capture, voice and anomaly detection, and sound classification
for Python, over the Mockingbird C ABI.

Quick start::

    import mockingbird

    with mockingbird.Engine(mockingbird.Config()) as engine:
        engine.start()
        for event in engine.events(timeout=10):
            match event:
                case mockingbird.Event(type=mockingbird.EventType.SPEECH_BEGIN):
                    print("voice")

One dial drives every detector::

    cfg = mockingbird.sensitive(0.6, neural_vad_model_path="silero_vad.onnx")

The library is found automatically in a repository checkout; elsewhere set
``MOCKINGBIRD_LIBRARY_PATH`` or call :func:`load_library`.
"""

from mockingbird._library import load_library, status_name, version
from mockingbird.anomaly_config import AnomalyConfig
from mockingbird.anomaly_reason import AnomalyReason
from mockingbird.backend import Backend
from mockingbird.capture_health import CaptureHealth
from mockingbird.codec import Codec
from mockingbird.config import Config, sensitive
from mockingbird.encoder_config import EncoderConfig
from mockingbird.engine import Engine
from mockingbird.error import MockingbirdError
from mockingbird.event import Event
from mockingbird.event_type import EventType
from mockingbird.input_device import InputDevice, input_devices
from mockingbird.packet import Packet
from mockingbird.packet_flag import PacketFlag
from mockingbird.performance_config import PerformanceConfig
from mockingbird.queue_config import QueueConfig
from mockingbird.segment_config import SegmentConfig
from mockingbird.sensitivity import Sensitivity
from mockingbird.sound_category import SoundCategory
from mockingbird.sound_class_config import SoundClassConfig
from mockingbird.stats import Stats
from mockingbird.status import Status
from mockingbird.vad_config import VadConfig
from mockingbird.vad_tier import VadTier
from mockingbird.voice_processing import VoiceProcessing
from mockingbird.wav import WAV_HEADER_BYTES, wav_from_pcm16
from mockingbird.worker_health import WorkerHealth

__all__ = [
    "AnomalyConfig",
    "AnomalyReason",
    "MockingbirdError",
    "Backend",
    "CaptureHealth",
    "Codec",
    "Config",
    "EncoderConfig",
    "Engine",
    "Event",
    "EventType",
    "InputDevice",
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
    "VoiceProcessing",
    "WAV_HEADER_BYTES",
    "WorkerHealth",
    "input_devices",
    "load_library",
    "sensitive",
    "status_name",
    "version",
    "wav_from_pcm16",
]

__version__ = "0.1.0"
