"""ctypes mirror of ``auris_packet_t``; ``data`` is a borrowed pointer."""

import ctypes


class PacketStruct(ctypes.Structure):
    """Mirrors ``auris_packet_t``."""

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
