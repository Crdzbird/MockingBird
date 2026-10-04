"""Prototypes of every native entry point the binding calls."""

import ctypes

from auris._config_struct import ConfigStruct
from auris._event_struct import EventStruct
from auris._packet_struct import PacketStruct
from auris._stats_struct import StatsStruct

EnginePtr = ctypes.c_void_p
_INT16_PTR = ctypes.POINTER(ctypes.c_int16)
SIGNATURES = [
    ("auris_config_defaults", [ctypes.POINTER(ConfigStruct)], ctypes.c_int),
    ("auris_create", [ctypes.POINTER(ConfigStruct), ctypes.POINTER(EnginePtr)], ctypes.c_int),
    ("auris_destroy", [EnginePtr], None),
    ("auris_start", [EnginePtr], ctypes.c_int),
    ("auris_stop", [EnginePtr], ctypes.c_int),
    ("auris_poll_event", [EnginePtr, ctypes.POINTER(EventStruct)], ctypes.c_int),
    ("auris_drain", [EnginePtr, ctypes.POINTER(PacketStruct)], ctypes.c_int),
    ("auris_release", [EnginePtr, ctypes.POINTER(PacketStruct)], ctypes.c_int),
    ("auris_get_stats", [EnginePtr, ctypes.POINTER(StatsStruct)], ctypes.c_int),
    ("auris_push_pcm", [EnginePtr, _INT16_PTR, ctypes.c_uint32, ctypes.c_uint64], ctypes.c_int),
    ("auris_read_pcm", [EnginePtr, _INT16_PTR, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)], ctypes.c_int),
    ("auris_update_config", [EnginePtr, ctypes.POINTER(ConfigStruct)], ctypes.c_int),
    ("auris_status_str", [ctypes.c_int], ctypes.c_char_p),
    ("auris_version_string", [], ctypes.c_char_p),
]
