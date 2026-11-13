"""Prototypes of the Opus decoder and player entry points."""

import ctypes

from mockingbird._player_config_struct import PlayerConfigStruct
from mockingbird._player_stats_struct import PlayerStatsStruct

DecoderPtr = ctypes.c_void_p
PlayerPtr = ctypes.c_void_p
_BYTES = ctypes.POINTER(ctypes.c_uint8)
_INT16_PTR = ctypes.POINTER(ctypes.c_int16)
_U32_PTR = ctypes.POINTER(ctypes.c_uint32)
_PLAYER_CONTROLS = ["start", "pause", "resume", "stop", "flush"]
PLAYBACK_SIGNATURES = [
    ("mockingbird_decoder_create", [ctypes.c_uint32, ctypes.c_uint16, ctypes.POINTER(DecoderPtr)], ctypes.c_int),
    ("mockingbird_decoder_destroy", [DecoderPtr], None),
    ("mockingbird_decoder_decode", [DecoderPtr, _BYTES, ctypes.c_uint32, _INT16_PTR, ctypes.c_uint32, _U32_PTR], ctypes.c_int),
    ("mockingbird_decoder_conceal", [DecoderPtr, ctypes.c_uint32, _INT16_PTR, ctypes.c_uint32, _U32_PTR], ctypes.c_int),
    ("mockingbird_decoder_reset", [DecoderPtr], ctypes.c_int),
    ("mockingbird_player_config_defaults", [ctypes.POINTER(PlayerConfigStruct)], ctypes.c_int),
    ("mockingbird_player_create", [ctypes.POINTER(PlayerConfigStruct), ctypes.POINTER(PlayerPtr)], ctypes.c_int),
    ("mockingbird_player_destroy", [PlayerPtr], None),
    ("mockingbird_player_push", [PlayerPtr, ctypes.c_int, _BYTES, ctypes.c_uint32, ctypes.c_uint64, ctypes.c_uint32], ctypes.c_int),
    ("mockingbird_player_read", [PlayerPtr, _INT16_PTR, ctypes.c_uint32], ctypes.c_int),
    ("mockingbird_player_set_volume", [PlayerPtr, ctypes.c_float], ctypes.c_int),
    ("mockingbird_player_set_muted", [PlayerPtr, ctypes.c_int], ctypes.c_int),
    ("mockingbird_player_get_stats", [PlayerPtr, ctypes.POINTER(PlayerStatsStruct)], ctypes.c_int),
] + [(f"mockingbird_player_{op}", [PlayerPtr], ctypes.c_int) for op in _PLAYER_CONTROLS]
