"""The player structs match the native layout and defaults."""

import ctypes
import unittest

from helpers import mockingbird
from mockingbird._library import library
from mockingbird._player_config_struct import PlayerConfigStruct
from mockingbird._player_stats_struct import PlayerStatsStruct
from opus_packets import host_player


def written_extent(fill) -> int:
    """How many bytes a native ``*out = {...}`` writer touched in a sentinel buffer."""
    raw = (ctypes.c_uint8 * 256)(*([0xAA] * 256))
    fill(ctypes.addressof(raw))
    return max(i for i in range(256) if raw[i] != 0xAA) + 1


class PlayerStructTest(unittest.TestCase):
    def test_sizes(self) -> None:
        self.assertEqual(ctypes.sizeof(PlayerConfigStruct), 44)
        self.assertEqual(ctypes.sizeof(PlayerStatsStruct), 96)

    def test_native_writers_fill_exactly_the_struct(self) -> None:
        lib = library()
        cfg = lambda a: lib.mockingbird_player_config_defaults(ctypes.cast(a, ctypes.POINTER(PlayerConfigStruct)))
        self.assertEqual(written_extent(cfg), ctypes.sizeof(PlayerConfigStruct))
        with host_player() as p:
            st = lambda a: lib.mockingbird_player_get_stats(p._handle, ctypes.cast(a, ctypes.POINTER(PlayerStatsStruct)))
            self.assertEqual(written_extent(st), ctypes.sizeof(PlayerStatsStruct))

    def test_python_defaults_match_native(self) -> None:
        native = PlayerConfigStruct()
        self.assertEqual(library().mockingbird_player_config_defaults(ctypes.byref(native)), 0)
        cfg = mockingbird.PlayerConfig()
        self.assertEqual(int(cfg.backend), native.backend)
        for name in ("sample_rate_hz", "channels", "target_delay_ms", "max_delay_ms", "conceal_max_ms", "queue_packets"):
            self.assertEqual(getattr(cfg, name), getattr(native, name), name)
        self.assertEqual(list(native._reserved), [0, 0, 0, 0])
        self.assertIs(mockingbird.PlayerStats().state, mockingbird.PlayerState.STOPPED)
