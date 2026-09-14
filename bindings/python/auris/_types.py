"""Public enums, records, and the YAMNet category helpers."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum


class Backend(IntEnum):
    """Capture source. Mirrors ``auris_backend_t``."""

    AUTO = 0
    MINIAUDIO = 1
    OBOE = 2
    COREAUDIO = 3
    HOST_PCM = 4  #: no internal capture; the host calls :meth:`Engine.push_pcm`


class VadTier(IntEnum):
    """Voice-activity detector. Both tiers are language-agnostic."""

    FAST = 0  #: libfvad (GMM), zero-dependency
    NEURAL = 1  #: Silero via ONNX Runtime, build-flag gated


class Codec(IntEnum):
    """Output encoding. Mirrors ``auris_codec_t``."""

    PCM_S16LE = 0
    OPUS = 1
    FLAC = 2  #: declared by the ABI, not yet implemented


class EventType(IntEnum):
    """Mirrors ``auris_event_type_t``."""

    SPEECH_BEGIN = 1
    SPEECH_END = 2
    ANOMALY = 3
    SEGMENT_BEGIN = 4
    SEGMENT_END = 5
    OVERRUN = 6
    BACKEND_ERROR = 7
    SOUND_CLASS = 8
    DEGRADATION = 9
    """Adaptive degradation changed level: ``reason_mask`` is the new level
    (0..3), ``confidence`` the worker's busy fraction."""


class Status(IntEnum):
    """Mirrors ``auris_status_t``."""

    OK = 0
    ERR_INVALID_ARG = -1
    ERR_INVALID_STATE = -2
    ERR_NO_MEMORY = -3
    ERR_BACKEND = -4
    ERR_NOT_RUNNING = -5
    ERR_ALREADY_RUNNING = -6
    ERR_QUEUE_EMPTY = -7
    ERR_PERMISSION = -8
    ERR_UNSUPPORTED = -9
    ERR_INTERNAL = -10


# Anomaly trigger bits, set in Event.reason_mask when type is ANOMALY.
ANOMALY_RMS = 1 << 0
ANOMALY_SPECTRAL_FLUX = 1 << 1
ANOMALY_CENTROID = 1 << 2

# Packet flag bits.
PACKET_FLAG_PREROLL = 1 << 0
PACKET_FLAG_POSTROLL = 1 << 1
PACKET_FLAG_KEYFRAME = 1 << 2
PACKET_FLAG_LAST = 1 << 3


class AurisError(RuntimeError):
    """A non-OK status from the native library.

    ``status`` carries the :class:`Status` so callers can branch on the cause::

        try:
            engine.start()
        except AurisError as exc:
            if exc.status is Status.ERR_PERMISSION:
                ...
    """

    def __init__(self, op: str, status: int, detail: str = "") -> None:
        try:
            self.status: Status | int = Status(status)
            name = self.status.name
        except ValueError:  # unknown code from a newer library
            self.status = status
            name = "UNKNOWN"
        self.op = op
        suffix = f": {detail}" if detail else ""
        super().__init__(f"auris: {op} failed: {name} ({status}){suffix}")


class SoundCategory(IntEnum):
    """Broad grouping of YAMNet's 521 fine-grained AudioSet classes.

    YAMNet splits one perceptual category across many labels; the human-voice
    cluster (0..12) covers Speech, Conversation, Narration, Babbling, Shout,
    Whispering, and so on. AudioSet is English/Western-biased, so non-English
    speech often lands on "Conversation" or "Narration" rather than the literal
    "Speech" — all of which map to :attr:`SPEECH`, which is what makes
    language-agnostic "is this voice?" decisions robust.
    """

    SPEECH = 0  #: human speech, any language (0..12)
    HUMAN_OTHER = 1  #: laughter, crying, singing, footsteps, applause (13..66)
    ANIMAL = 2  #: dogs, cats, birds, livestock (67..131)
    MUSIC = 3  #: music + instruments (132..282)
    NATURE = 4  #: wind, water, thunder, fire (283..299)
    OTHER = 5  #: vehicles, alarms, tools, glass, Silence, ...

    def __str__(self) -> str:  # friendlier printing
        return self.name.lower()


def category_for_yamnet_class(class_id: int) -> SoundCategory:
    """Map a YAMNet AudioSet class index to a broad :class:`SoundCategory`."""
    if 0 <= class_id <= 12:
        return SoundCategory.SPEECH
    if 13 <= class_id <= 66:
        return SoundCategory.HUMAN_OTHER
    if 67 <= class_id <= 131:
        return SoundCategory.ANIMAL
    if 132 <= class_id <= 282:
        return SoundCategory.MUSIC
    if 283 <= class_id <= 299:
        return SoundCategory.NATURE
    return SoundCategory.OTHER


def is_speech_class(class_id: int) -> bool:
    """True if the class is human speech in any language (cluster 0..12)."""
    return category_for_yamnet_class(class_id) is SoundCategory.SPEECH


def is_human_voice(class_id: int) -> bool:
    """True if a person's voice made the sound: speech in any language plus
    laughter, crying, and singing (classes 0..34).

    Excludes non-vocal human sounds (whistling, breathing, footsteps) and
    everything non-human.
    """
    return 0 <= class_id <= 34


@dataclass(frozen=True, slots=True)
class Event:
    """A detection event."""

    type: EventType
    segment_id: int
    timestamp_us: int
    reason_mask: int
    confidence: float

    @property
    def sound_class(self) -> int:
        """YAMNet AudioSet class index (only for :attr:`EventType.SOUND_CLASS`)."""
        return self.reason_mask

    @property
    def backend_error(self) -> Status | int:
        """The status carried by a :attr:`EventType.BACKEND_ERROR` event.

        The C ABI stores the negative status in an unsigned field, so this
        reinterprets the bits as signed.
        """
        signed = self.reason_mask - (1 << 32) if self.reason_mask >= (1 << 31) else self.reason_mask
        try:
            return Status(signed)
        except ValueError:
            return signed


@dataclass(frozen=True, slots=True)
class Packet:
    """One encoded audio packet. ``data`` is a Python-owned copy — the native
    zero-copy borrow is released immediately after the copy."""

    segment_id: int
    pts_us: int
    capture_ts_us: int
    flags: int
    codec: Codec
    data: bytes


@dataclass(frozen=True, slots=True)
class Stats:
    """Observability snapshot. Never carries audio."""

    frames_captured: int = 0
    frames_dropped_capture: int = 0
    packets_emitted: int = 0
    packets_dropped: int = 0
    segments_opened: int = 0
    segments_closed: int = 0
    pcm_ring_fill_frames: int = 0
    packet_queue_fill: int = 0
    input_rms_dbfs: float = 0.0
    input_peak_dbfs: float = 0.0
    tap_samples_dropped: int = 0

    #: Share of wall time the worker spent processing over the last ~1 s
    #: window, parts per million (1_000_000 = saturated).
    worker_busy_ppm: int = 0
    #: Longest single-frame processing time since start, microseconds.
    worker_max_frame_us: int = 0
    #: Worker thread parks/unparks — a battery proxy on mobile.
    worker_wakeups: int = 0
    #: YAMNet inferences that ran / were due but shed (silence gate or
    #: degradation).
    sound_class_inferences: int = 0
    sound_class_inferences_skipped: int = 0
    #: Frames RNNoise skipped because they were below the silence gate.
    preproc_frames_skipped: int = 0
    #: Adaptive-degradation level, 0 (nominal) .. 3 (classifier suspended).
    degradation_level: int = 0

    @property
    def worker_busy_fraction(self) -> float:
        """:attr:`worker_busy_ppm` as 0..1."""
        return self.worker_busy_ppm / 1_000_000


@dataclass(slots=True)
class Config:
    """Engine configuration. Mirrors ``auris_config_t``.

    Build one directly for full control, or with :func:`sensitive` for a single
    0..1 dial that drives every detector.
    """

    backend: Backend = Backend.AUTO
    sample_rate_hz: int = 16000
    channels: int = 1
    frame_ms: int = 20

    # Preprocess
    enable_rnnoise: bool = False

    # Voice activity detection (multilingual)
    enable_vad: bool = True
    vad_tier: VadTier = VadTier.FAST
    vad_aggressiveness: int = 2
    vad_neural_threshold: float = 0.5
    vad_hangover_ms: int = 300
    neural_vad_model_path: str | None = None

    # Anomaly gate
    enable_anomaly: bool = True
    anomaly_rms_z: float = 3.0
    anomaly_flux_z: float = 3.0
    anomaly_background_ms: int = 2000

    # Segmentation
    pre_roll_ms: int = 300
    post_roll_ms: int = 300
    segment_max_ms: int = 60000

    # Encoder
    codec: Codec = Codec.PCM_S16LE
    opus_bitrate_bps: int = 32000
    opus_vbr: bool = True
    opus_complexity: int = 5

    # Queues
    pcm_ring_frames: int = 1 << 14
    packet_queue_capacity: int = 256
    enable_wait_handle: bool = False

    # Sound classification (YAMNet, all 521 AudioSet classes)
    enable_sound_class: bool = False
    sound_class_model_path: str | None = None
    sound_class_threshold: float = 0.4
    sound_class_interval_ms: int = 1000

    # Continuous (ungated) PCM tap — see Engine.read_pcm
    enable_pcm_tap: bool = False
    pcm_tap_frames: int = 0

    #: Frames whose input RMS is below this (dBFS) are "quiet": RNNoise is
    #: skipped for them and YAMNet skips an inference whose whole 3 s window
    #: was quiet. VAD/anomaly are never gated; audio always flows. -60 is
    #: conservative; ~-50 also sheds quiet-room tone; <= -120 disables.
    silence_gate_dbfs: float = -60.0
    #: Shed YAMNet work under load (cadence x2, x4, then suspend) and restore
    #: it after a few calm seconds; emits :attr:`EventType.DEGRADATION`. Off
    #: by default because it reacts to wall-clock timing — enable for live
    #: capture, not for faster-than-real-time offline processing.
    enable_adaptive_degradation: bool = False
    #: Busy threshold, parts per million (700_000 = 70%).
    degrade_busy_ppm: int = 700_000


def sensitive(dial: float, **overrides: object) -> Config:
    """Build a :class:`Config` from one 0..1 sensitivity dial.

    0 is least sensitive (fewest detections and false positives), 1 is most
    sensitive. Voice detection stays multilingual regardless of the dial; it only
    changes how eagerly a frame is called speech. Enables all three detectors
    with neural (Silero) VAD by default.

    ==========================  ============
    Silero P(speech)            0.80 .. 0.20
    libfvad aggressiveness      3    .. 0
    anomaly RMS / flux z-score  5.0  .. 1.5
    YAMNet top-1 probability    0.70 .. 0.12
    ==========================  ============

    Any :class:`Config` field can be overridden by keyword::

        cfg = auris.sensitive(0.6, enable_rnnoise=True, codec=auris.Codec.OPUS)
    """
    s = min(1.0, max(0.0, float(dial)))

    def lerp(a: float, b: float) -> float:
        return a + (b - a) * s

    cfg = Config(
        vad_tier=VadTier.NEURAL,
        enable_vad=True,
        enable_anomaly=True,
        enable_sound_class=True,
        vad_aggressiveness=min(3, max(0, round(lerp(3, 0)))),
        vad_neural_threshold=lerp(0.80, 0.20),
        anomaly_rms_z=lerp(5.0, 1.5),
        anomaly_flux_z=lerp(5.0, 1.5),
        sound_class_threshold=lerp(0.70, 0.12),
    )
    for key, value in overrides.items():
        if not hasattr(cfg, key):
            raise TypeError(f"unknown Config field: {key!r}")
        setattr(cfg, key, value)
    return cfg
