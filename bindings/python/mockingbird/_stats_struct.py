"""ctypes mirror of ``mockingbird_stats_t``."""

import ctypes


class StatsStruct(ctypes.Structure):
    """Mirrors ``mockingbird_stats_t``."""

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
        ("voice_processing_active", ctypes.c_uint32),
        ("capture_restarts", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32 * 5),
    ]
