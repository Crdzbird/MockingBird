"""ctypes mirror of ``mockingbird_player_config_t``; field order is load-bearing."""

import ctypes


class PlayerConfigStruct(ctypes.Structure):
    """Mirrors ``mockingbird_player_config_t`` field for field."""

    _fields_ = [
        ("backend", ctypes.c_int),
        ("sample_rate_hz", ctypes.c_uint32),
        ("channels", ctypes.c_uint16),
        ("target_delay_ms", ctypes.c_uint32),
        ("max_delay_ms", ctypes.c_uint32),
        ("conceal_max_ms", ctypes.c_uint32),
        ("queue_packets", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32 * 4),
    ]
