"""ctypes mirror of ``mockingbird_device_t``."""

import ctypes


class DeviceStruct(ctypes.Structure):
    """Mirrors ``mockingbird_device_t``."""

    _fields_ = [
        ("id", ctypes.c_char * 256),
        ("name", ctypes.c_char * 256),
        ("is_default", ctypes.c_int),
        ("_reserved", ctypes.c_uint32 * 3),
    ]
