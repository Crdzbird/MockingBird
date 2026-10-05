"""ctypes mirror of ``mockingbird_event_t``."""

import ctypes


class EventStruct(ctypes.Structure):
    """Mirrors ``mockingbird_event_t``."""

    _fields_ = [
        ("type", ctypes.c_int),
        ("segment_id", ctypes.c_uint64),
        ("timestamp_us", ctypes.c_uint64),
        ("reason_mask", ctypes.c_uint32),
        ("confidence", ctypes.c_float),
        ("_reserved", ctypes.c_uint32),
    ]
