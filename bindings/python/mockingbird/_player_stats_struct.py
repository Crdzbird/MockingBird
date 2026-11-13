"""ctypes mirror of ``mockingbird_player_stats_t``."""

import ctypes


class PlayerStatsStruct(ctypes.Structure):
    """Mirrors ``mockingbird_player_stats_t`` field for field."""

    _fields_ = [
        ("packets_received", ctypes.c_uint64),
        ("packets_played", ctypes.c_uint64),
        ("packets_concealed", ctypes.c_uint64),
        ("packets_dropped", ctypes.c_uint64),
        ("packets_late", ctypes.c_uint64),
        ("underruns", ctypes.c_uint64),
        ("frames_rendered", ctypes.c_uint64),
        ("buffered_packets", ctypes.c_uint32),
        ("buffered_ms", ctypes.c_uint32),
        ("state", ctypes.c_uint32),
        ("output_restarts", ctypes.c_uint32),
        ("_reserved", ctypes.c_uint32 * 6),
    ]
