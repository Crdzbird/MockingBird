"""Auris — native audio capture, denoise, voice/anomaly detection, and sound
classification, driven from Python through the library's C ABI.

Quick start::

    import auris

    with auris.Engine(auris.Config()) as engine:   # default mic, fast VAD + anomaly
        engine.start()
        for event in engine.events(timeout=10):
            if event.type is auris.EventType.SPEECH_BEGIN:
                print("voice detected")

One dial tunes every detector::

    cfg = auris.sensitive(0.6, neural_vad_model_path="silero_vad.onnx")

Offline / file processing (no microphone) via the HOST_PCM backend::

    cfg = auris.Config(backend=auris.Backend.HOST_PCM)
    with auris.Engine(cfg) as engine:
        engine.start()
        engine.push_pcm(samples)          # int16 mono
        print(engine.poll_events())

The library is located automatically in a repo checkout (``build/libauris.*``);
elsewhere set ``AURIS_LIBRARY_PATH`` or call :func:`load_library`.
"""

from ._engine import Engine, load_library, status_name, version
from ._types import (
    ANOMALY_CENTROID,
    ANOMALY_RMS,
    ANOMALY_SPECTRAL_FLUX,
    PACKET_FLAG_KEYFRAME,
    PACKET_FLAG_LAST,
    PACKET_FLAG_POSTROLL,
    PACKET_FLAG_PREROLL,
    AurisError,
    Backend,
    Codec,
    Config,
    Event,
    EventType,
    Packet,
    SoundCategory,
    Stats,
    Status,
    VadTier,
    category_for_yamnet_class,
    is_human_voice,
    is_speech_class,
    sensitive,
)

__all__ = [
    "ANOMALY_CENTROID",
    "ANOMALY_RMS",
    "ANOMALY_SPECTRAL_FLUX",
    "PACKET_FLAG_KEYFRAME",
    "PACKET_FLAG_LAST",
    "PACKET_FLAG_POSTROLL",
    "PACKET_FLAG_PREROLL",
    "AurisError",
    "Backend",
    "Codec",
    "Config",
    "Engine",
    "Event",
    "EventType",
    "Packet",
    "SoundCategory",
    "Stats",
    "Status",
    "VadTier",
    "category_for_yamnet_class",
    "is_human_voice",
    "is_speech_class",
    "load_library",
    "sensitive",
    "status_name",
    "version",
]

__version__ = "0.1.0"
