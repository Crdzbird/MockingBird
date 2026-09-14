"""The Engine: a Pythonic owner of one native ``auris_engine_t``."""

from __future__ import annotations

import ctypes
import threading
from collections.abc import Iterator, Sequence
from typing import Any

from . import _ffi
from ._types import (
    AurisError,
    Codec,
    Config,
    Event,
    EventType,
    Packet,
    Stats,
    Status,
)

_lib: ctypes.CDLL | None = None
_lib_lock = threading.Lock()


def _library() -> ctypes.CDLL:
    global _lib
    with _lib_lock:
        if _lib is None:
            _lib = _ffi.bind(_ffi.load_library())
        return _lib


def load_library(path: str | None = None) -> None:
    """Load libauris from an explicit path.

    Call before constructing an :class:`Engine` when the library is not on the
    default search path (the ``AURIS_LIBRARY_PATH`` environment variable works
    too).
    """
    global _lib
    with _lib_lock:
        _lib = _ffi.bind(_ffi.load_library(path))


def version() -> str:
    """The ``X.Y.Z`` version of the linked libauris."""
    return _library().auris_version_string().decode()


def status_name(status: int) -> str:
    """Human-readable name for a status code."""
    return _library().auris_status_str(int(status)).decode()


def _check(lib: ctypes.CDLL, code: int, op: str) -> None:
    if code != Status.OK:
        raise AurisError(op, code, lib.auris_status_str(code).decode())


def _to_c_config(cfg: Config, lib: ctypes.CDLL) -> _ffi.AurisConfig:
    """Marshal a :class:`Config` into the C struct, starting from the library's
    own defaults so reserved fields are correctly zeroed."""
    c = _ffi.AurisConfig()
    _check(lib, lib.auris_config_defaults(ctypes.byref(c)), "auris_config_defaults")

    c.backend = int(cfg.backend)
    c.sample_rate_hz = cfg.sample_rate_hz
    c.channels = cfg.channels
    c.frame_ms = cfg.frame_ms

    c.enable_rnnoise = int(bool(cfg.enable_rnnoise))

    c.enable_vad = int(bool(cfg.enable_vad))
    c.vad_tier = int(cfg.vad_tier)
    c.vad_aggressiveness = cfg.vad_aggressiveness
    c.vad_neural_threshold = cfg.vad_neural_threshold
    c.vad_hangover_ms = cfg.vad_hangover_ms

    c.enable_anomaly = int(bool(cfg.enable_anomaly))
    c.anomaly_rms_z = cfg.anomaly_rms_z
    c.anomaly_flux_z = cfg.anomaly_flux_z
    c.anomaly_background_ms = cfg.anomaly_background_ms

    c.pre_roll_ms = cfg.pre_roll_ms
    c.post_roll_ms = cfg.post_roll_ms
    c.segment_max_ms = cfg.segment_max_ms

    c.codec = int(cfg.codec)
    c.opus_bitrate_bps = cfg.opus_bitrate_bps
    c.opus_vbr = int(bool(cfg.opus_vbr))
    c.opus_complexity = cfg.opus_complexity

    c.pcm_ring_frames = cfg.pcm_ring_frames
    c.packet_queue_capacity = cfg.packet_queue_capacity
    c.enable_wait_handle = int(bool(cfg.enable_wait_handle))

    c.enable_sound_class = int(bool(cfg.enable_sound_class))
    c.sound_class_threshold = cfg.sound_class_threshold
    c.sound_class_interval_ms = cfg.sound_class_interval_ms

    c.enable_pcm_tap = int(bool(cfg.enable_pcm_tap))
    c.pcm_tap_frames = cfg.pcm_tap_frames

    c.silence_gate_dbfs = cfg.silence_gate_dbfs
    c.enable_adaptive_degradation = int(bool(cfg.enable_adaptive_degradation))
    c.degrade_busy_ppm = cfg.degrade_busy_ppm

    # The engine deep-copies any string it retains, but the bytes must outlive
    # this call — keep them alive on the struct object.
    c._keepalive = []  # type: ignore[attr-defined]
    if cfg.neural_vad_model_path:
        buf = cfg.neural_vad_model_path.encode()
        c._keepalive.append(buf)  # type: ignore[attr-defined]
        c.neural_vad_model_path = buf
    if cfg.sound_class_model_path:
        buf = cfg.sound_class_model_path.encode()
        c._keepalive.append(buf)  # type: ignore[attr-defined]
        c.sound_class_model_path = buf
    return c


class Engine:
    """A running (or runnable) Auris pipeline.

    Use it as a context manager so the native handle is always released::

        with auris.Engine(auris.Config()) as engine:
            engine.start()
            for event in engine.events(timeout=5.0):
                print(event)

    Not safe to drive from several threads at once: keep control calls and
    draining on one thread (the same single-consumer contract the C ABI states).
    """

    def __init__(self, config: Config | None = None) -> None:
        self._lib = _library()
        self._config = config or Config()
        self._handle = _ffi._EnginePtr()
        c = _to_c_config(self._config, self._lib)
        _check(
            self._lib,
            self._lib.auris_create(ctypes.byref(c), ctypes.byref(self._handle)),
            "auris_create",
        )
        self._running = False
        self._closed = False

    # --- lifecycle ---------------------------------------------------------

    def __enter__(self) -> Engine:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def __del__(self) -> None:  # best-effort backstop
        try:
            self.close()
        except Exception:  # noqa: BLE001 - never raise from __del__
            pass

    @property
    def config(self) -> Config:
        """The configuration this engine is running with."""
        return self._config

    @property
    def is_running(self) -> bool:
        return self._running

    def _require_open(self) -> None:
        if self._closed:
            raise AurisError("engine", Status.ERR_INVALID_STATE, "engine is closed")

    def start(self) -> None:
        """Begin capture and detection."""
        self._require_open()
        _check(self._lib, self._lib.auris_start(self._handle), "auris_start")
        self._running = True

    def stop(self) -> None:
        """Halt capture. Queued packets stay drainable; the engine can restart."""
        self._require_open()
        _check(self._lib, self._lib.auris_stop(self._handle), "auris_stop")
        self._running = False

    def close(self) -> None:
        """Stop (if needed) and destroy the native engine. Idempotent."""
        if self._closed:
            return
        self._closed = True
        self._running = False
        if self._handle:
            self._lib.auris_destroy(self._handle)  # auto-stops if running
            self._handle = _ffi._EnginePtr()

    # --- audio in ----------------------------------------------------------

    def push_pcm(self, samples: Sequence[int] | Any, capture_ts_us: int = 0) -> None:
        """Feed int16 mono PCM into a ``HOST_PCM`` engine.

        Accepts anything buffer-like (``bytes``, ``array('h')``, a numpy int16
        array) or a plain sequence of ints. ``capture_ts_us`` is the monotonic
        timestamp of the first sample.
        """
        self._require_open()
        buf, count = _as_int16_buffer(samples)
        if count == 0:
            return
        _check(
            self._lib,
            self._lib.auris_push_pcm(self._handle, buf, count, capture_ts_us),
            "auris_push_pcm",
        )

    # --- audio / detections out --------------------------------------------

    def poll_event(self) -> Event | None:
        """Pop one event, or ``None`` when the queue is empty."""
        self._require_open()
        ev = _ffi.AurisEvent()
        code = self._lib.auris_poll_event(self._handle, ctypes.byref(ev))
        if code == Status.ERR_QUEUE_EMPTY:
            return None
        _check(self._lib, code, "auris_poll_event")
        return Event(
            type=EventType(ev.type) if ev.type in EventType._value2member_map_ else ev.type,
            segment_id=ev.segment_id,
            timestamp_us=ev.timestamp_us,
            reason_mask=ev.reason_mask,
            confidence=ev.confidence,
        )

    def poll_events(self) -> list[Event]:
        """Drain every buffered event."""
        out: list[Event] = []
        while (ev := self.poll_event()) is not None:
            out.append(ev)
        return out

    def drain_packet(self) -> Packet | None:
        """Pop one encoded packet, or ``None`` when the queue is empty.

        The bytes are copied into Python before the native borrow is released,
        so the returned packet is safe to keep and forward.
        """
        self._require_open()
        pkt = _ffi.AurisPacket()
        code = self._lib.auris_drain(self._handle, ctypes.byref(pkt))
        if code == Status.ERR_QUEUE_EMPTY:
            return None
        _check(self._lib, code, "auris_drain")
        data = bytes(bytearray(pkt.data[: pkt.size])) if pkt.size and pkt.data else b""
        out = Packet(
            segment_id=pkt.segment_id,
            pts_us=pkt.pts_us,
            capture_ts_us=pkt.capture_ts_us,
            flags=pkt.flags,
            codec=Codec(pkt.codec) if pkt.codec in Codec._value2member_map_ else pkt.codec,
            data=data,
        )
        _check(self._lib, self._lib.auris_release(self._handle, ctypes.byref(pkt)), "auris_release")
        return out

    def drain_packets(self) -> list[Packet]:
        """Drain every buffered packet."""
        out: list[Packet] = []
        while (pkt := self.drain_packet()) is not None:
            out.append(pkt)
        return out

    def read_pcm(self, max_samples: int = 4096) -> bytes:
        """Drain the continuous PCM tap (requires ``config.enable_pcm_tap``).

        Returns raw little-endian int16 mono bytes — empty when nothing is
        buffered. Unlike packets, the tap is NOT gated by segments: it carries
        every processed frame, so you can archive or visualise the whole stream
        while still receiving detections.
        """
        self._require_open()
        if max_samples <= 0:
            return b""
        buf = (ctypes.c_int16 * max_samples)()
        written = ctypes.c_uint32(0)
        _check(
            self._lib,
            self._lib.auris_read_pcm(self._handle, buf, max_samples, ctypes.byref(written)),
            "auris_read_pcm",
        )
        if written.value == 0:
            return b""
        return bytes(memoryview(buf).cast("B")[: written.value * 2])

    def events(self, timeout: float | None = None, poll_interval: float = 0.02) -> Iterator[Event]:
        """Yield events as they arrive, polling underneath.

        Stops after ``timeout`` seconds (``None`` runs until the engine stops or
        is closed). Convenient for scripts::

            for event in engine.events(timeout=10):
                print(event)
        """
        import time

        deadline = None if timeout is None else time.monotonic() + timeout
        while not self._closed:
            for ev in self.poll_events():
                yield ev
            if deadline is not None and time.monotonic() >= deadline:
                return
            time.sleep(poll_interval)

    # --- observability / control -------------------------------------------

    def stats(self) -> Stats:
        """Snapshot the observability counters and input level."""
        self._require_open()
        st = _ffi.AurisStats()
        _check(self._lib, self._lib.auris_get_stats(self._handle, ctypes.byref(st)), "auris_get_stats")
        return Stats(
            frames_captured=st.frames_captured,
            frames_dropped_capture=st.frames_dropped_capture,
            packets_emitted=st.packets_emitted,
            packets_dropped=st.packets_dropped,
            segments_opened=st.segments_opened,
            segments_closed=st.segments_closed,
            pcm_ring_fill_frames=st.pcm_ring_fill_frames,
            packet_queue_fill=st.packet_queue_fill,
            input_rms_dbfs=st.input_rms_dbfs,
            input_peak_dbfs=st.input_peak_dbfs,
            tap_samples_dropped=st.tap_samples_dropped,
            worker_busy_ppm=st.worker_busy_ppm,
            worker_max_frame_us=st.worker_max_frame_us,
            worker_wakeups=st.worker_wakeups,
            sound_class_inferences=st.sound_class_inferences,
            sound_class_inferences_skipped=st.sound_class_inferences_skipped,
            preproc_frames_skipped=st.preproc_frames_skipped,
            degradation_level=st.degradation_level,
        )

    def update_config(self, config: Config) -> None:
        """Retune in place — no teardown, no model reload, detector state kept.

        Only the tunable subset applies (detector enables, ``vad_aggressiveness``,
        ``vad_neural_threshold``, ``vad_hangover_ms``, the anomaly z-scores, and
        ``sound_class_threshold``); every other field is structural and must match
        the config the engine was created with. The usual pattern is to mutate the
        config you already have::

            cfg.anomaly_rms_z = 2.0
            engine.update_config(cfg)

        Raises :class:`AurisError` if a structural field differs or if it tries to
        switch on a detector that was off at create time.
        """
        self._require_open()
        c = _to_c_config(config, self._lib)
        _check(
            self._lib,
            self._lib.auris_update_config(self._handle, ctypes.byref(c)),
            "auris_update_config",
        )
        self._config = config


def _as_int16_buffer(samples: Any) -> tuple[Any, int]:
    """Coerce buffer-likes / sequences into (ctypes int16 pointer, count)."""
    # Anything exposing the buffer protocol with int16 items (numpy, array('h'),
    # memoryview) can be passed through without copying element by element.
    try:
        mv = memoryview(samples)
    except TypeError:
        seq = list(samples)
        arr = (ctypes.c_int16 * len(seq))(*seq)
        return arr, len(seq)
    if mv.ndim != 1:
        mv = mv.cast("B").cast("h")
    if mv.format not in ("h", "B", "b"):
        mv = mv.cast("B").cast("h")
    elif mv.format != "h":
        mv = mv.cast("h")
    count = mv.shape[0]
    ptr = ctypes.cast(
        (ctypes.c_char * (count * 2)).from_buffer_copy(mv.tobytes()),
        ctypes.POINTER(ctypes.c_int16),
    )
    return ptr, count
