"""ctypes layer: struct mirrors, prototypes, and library loading.

Everything here mirrors ``include/auris/auris.h`` field-for-field and in the same
order — the layouts are load-bearing, so keep them in sync with the header. The
package's test suite asserts round-tripping through ``auris_config_defaults`` to
catch a drift.
"""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

# --- library discovery -------------------------------------------------------

_LIB_ENV = "AURIS_LIBRARY_PATH"


def _candidate_names() -> list[str]:
    if sys.platform == "darwin":
        return ["libauris.dylib", "libauris.0.1.0.dylib"]
    if sys.platform == "win32":
        return ["auris.dll", "libauris.dll"]
    return ["libauris.so", "libauris.so.0.1.0"]


def _candidate_paths() -> list[Path]:
    paths: list[Path] = []
    override = os.environ.get(_LIB_ENV)
    if override:
        paths.append(Path(override))
    # Repo layout: bindings/python/auris/_ffi.py -> <repo>/build/<lib>
    repo_build = Path(__file__).resolve().parents[3] / "build"
    for name in _candidate_names():
        paths.append(repo_build / name)
    # Fall back to the loader's own search path.
    paths.extend(Path(n) for n in _candidate_names())
    return paths


def load_library(path: str | os.PathLike[str] | None = None) -> ctypes.CDLL:
    """Load libauris, raising a helpful error if it cannot be found."""
    tried: list[str] = []
    candidates = [Path(path)] if path is not None else _candidate_paths()
    for cand in candidates:
        try:
            return ctypes.CDLL(str(cand))
        except OSError:
            tried.append(str(cand))
    raise OSError(
        "could not load libauris. Build it with:\n"
        "    cmake -S . -B build -DAURIS_BUILD_SHARED=ON && cmake --build build -j\n"
        f"or set {_LIB_ENV} to the shared library.\nTried:\n  "
        + "\n  ".join(tried)
    )


# --- struct mirrors ----------------------------------------------------------


class AurisConfig(ctypes.Structure):
    """Mirrors ``auris_config_t``."""

    _fields_ = [
        ("backend", ctypes.c_int),
        ("sample_rate_hz", ctypes.c_uint32),
        ("channels", ctypes.c_uint16),
        ("sample_format", ctypes.c_int),
        ("frame_ms", ctypes.c_uint32),
        ("enable_rnnoise", ctypes.c_int),
        ("enable_vad", ctypes.c_int),
        ("vad_tier", ctypes.c_int),
        ("vad_aggressiveness", ctypes.c_int),
        ("vad_neural_threshold", ctypes.c_float),
        ("vad_hangover_ms", ctypes.c_uint32),
        ("enable_anomaly", ctypes.c_int),
        ("anomaly_rms_z", ctypes.c_float),
        ("anomaly_flux_z", ctypes.c_float),
        ("anomaly_background_ms", ctypes.c_uint32),
        ("pre_roll_ms", ctypes.c_uint32),
        ("post_roll_ms", ctypes.c_uint32),
        ("segment_max_ms", ctypes.c_uint32),
        ("codec", ctypes.c_int),
        ("opus_bitrate_bps", ctypes.c_uint32),
        ("opus_vbr", ctypes.c_int),
        ("opus_complexity", ctypes.c_int),
        ("pcm_ring_frames", ctypes.c_uint32),
        ("packet_queue_capacity", ctypes.c_uint32),
        ("enable_wait_handle", ctypes.c_int),
        ("neural_vad_model_path", ctypes.c_char_p),
        ("enable_sound_class", ctypes.c_int),
        ("sound_class_model_path", ctypes.c_char_p),
        ("sound_class_threshold", ctypes.c_float),
        ("sound_class_interval_ms", ctypes.c_uint32),
        ("enable_pcm_tap", ctypes.c_int),
        ("pcm_tap_frames", ctypes.c_uint32),
        ("silence_gate_dbfs", ctypes.c_float),
        ("enable_adaptive_degradation", ctypes.c_int),
        ("degrade_busy_ppm", ctypes.c_uint32),
        ("worker_batch_frames", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32 * 4),
    ]


class AurisPacket(ctypes.Structure):
    """Mirrors ``auris_packet_t``. ``data`` is a borrowed pointer."""

    _fields_ = [
        ("segment_id", ctypes.c_uint64),
        ("pts_us", ctypes.c_uint64),
        ("capture_ts_us", ctypes.c_uint64),
        ("flags", ctypes.c_uint32),
        ("codec", ctypes.c_int),
        ("data", ctypes.POINTER(ctypes.c_uint8)),
        ("size", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32),
    ]


class AurisEvent(ctypes.Structure):
    """Mirrors ``auris_event_t``."""

    _fields_ = [
        ("type", ctypes.c_int),
        ("segment_id", ctypes.c_uint64),
        ("timestamp_us", ctypes.c_uint64),
        ("reason_mask", ctypes.c_uint32),
        ("confidence", ctypes.c_float),
        ("_reserved", ctypes.c_uint32),
    ]


class AurisStats(ctypes.Structure):
    """Mirrors ``auris_stats_t``."""

    _fields_ = [
        ("frames_captured", ctypes.c_uint64),
        ("frames_dropped_capture", ctypes.c_uint64),
        ("packets_emitted", ctypes.c_uint64),
        ("packets_dropped", ctypes.c_uint64),
        ("segments_opened", ctypes.c_uint64),
        ("segments_closed", ctypes.c_uint64),
        ("pcm_ring_fill_frames", ctypes.c_uint32),
        ("packet_queue_fill", ctypes.c_uint32),
        ("input_rms_dbfs", ctypes.c_float),
        ("input_peak_dbfs", ctypes.c_float),
        ("tap_samples_dropped", ctypes.c_uint64),
        ("worker_busy_ppm", ctypes.c_uint32),
        ("worker_max_frame_us", ctypes.c_uint32),
        ("worker_wakeups", ctypes.c_uint64),
        ("sound_class_inferences", ctypes.c_uint64),
        ("sound_class_inferences_skipped", ctypes.c_uint64),
        ("preproc_frames_skipped", ctypes.c_uint64),
        ("degradation_level", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32 * 7),
    ]


_EnginePtr = ctypes.c_void_p


def bind(lib: ctypes.CDLL) -> ctypes.CDLL:
    """Attach argtypes/restype to every entry point we call."""
    sig = [
        ("auris_config_defaults", [ctypes.POINTER(AurisConfig)], ctypes.c_int),
        ("auris_create", [ctypes.POINTER(AurisConfig), ctypes.POINTER(_EnginePtr)], ctypes.c_int),
        ("auris_destroy", [_EnginePtr], None),
        ("auris_start", [_EnginePtr], ctypes.c_int),
        ("auris_stop", [_EnginePtr], ctypes.c_int),
        ("auris_poll_event", [_EnginePtr, ctypes.POINTER(AurisEvent)], ctypes.c_int),
        ("auris_drain", [_EnginePtr, ctypes.POINTER(AurisPacket)], ctypes.c_int),
        ("auris_release", [_EnginePtr, ctypes.POINTER(AurisPacket)], ctypes.c_int),
        ("auris_get_stats", [_EnginePtr, ctypes.POINTER(AurisStats)], ctypes.c_int),
        (
            "auris_push_pcm",
            [_EnginePtr, ctypes.POINTER(ctypes.c_int16), ctypes.c_uint32, ctypes.c_uint64],
            ctypes.c_int,
        ),
        (
            "auris_read_pcm",
            [_EnginePtr, ctypes.POINTER(ctypes.c_int16), ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)],
            ctypes.c_int,
        ),
        ("auris_update_config", [_EnginePtr, ctypes.POINTER(AurisConfig)], ctypes.c_int),
        ("auris_status_str", [ctypes.c_int], ctypes.c_char_p),
        ("auris_version_string", [], ctypes.c_char_p),
    ]
    for name, argtypes, restype in sig:
        fn = getattr(lib, name)
        fn.argtypes = argtypes
        fn.restype = restype
    return lib
