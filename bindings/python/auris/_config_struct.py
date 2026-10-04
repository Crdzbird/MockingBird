"""ctypes mirror of ``auris_config_t``; field order is load-bearing."""

import ctypes


class ConfigStruct(ctypes.Structure):
    """Mirrors ``include/auris/auris.h`` field for field."""

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
