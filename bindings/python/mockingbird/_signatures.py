"""Prototypes of every native entry point the binding calls."""

import ctypes

from mockingbird._config_struct import ConfigStruct
from mockingbird._event_struct import EventStruct
from mockingbird._packet_struct import PacketStruct
from mockingbird._stats_struct import StatsStruct

EnginePtr = ctypes.c_void_p
_INT16_PTR = ctypes.POINTER(ctypes.c_int16)
SIGNATURES = [
    ("mockingbird_config_defaults", [ctypes.POINTER(ConfigStruct)], ctypes.c_int),
    ("mockingbird_create", [ctypes.POINTER(ConfigStruct), ctypes.POINTER(EnginePtr)], ctypes.c_int),
    ("mockingbird_destroy", [EnginePtr], None),
    ("mockingbird_start", [EnginePtr], ctypes.c_int),
    ("mockingbird_stop", [EnginePtr], ctypes.c_int),
    ("mockingbird_poll_event", [EnginePtr, ctypes.POINTER(EventStruct)], ctypes.c_int),
    ("mockingbird_drain", [EnginePtr, ctypes.POINTER(PacketStruct)], ctypes.c_int),
    ("mockingbird_release", [EnginePtr, ctypes.POINTER(PacketStruct)], ctypes.c_int),
    ("mockingbird_get_stats", [EnginePtr, ctypes.POINTER(StatsStruct)], ctypes.c_int),
    ("mockingbird_push_pcm", [EnginePtr, _INT16_PTR, ctypes.c_uint32, ctypes.c_uint64], ctypes.c_int),
    ("mockingbird_read_pcm", [EnginePtr, _INT16_PTR, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)], ctypes.c_int),
    ("mockingbird_update_config", [EnginePtr, ctypes.POINTER(ConfigStruct)], ctypes.c_int),
    ("mockingbird_status_str", [ctypes.c_int], ctypes.c_char_p),
    ("mockingbird_version_string", [], ctypes.c_char_p),
]
