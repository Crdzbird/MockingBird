"""The jitter-buffered player on HOST_PCM, fed the engine's Opus packets."""

import unittest

from helpers import FRAME, mockingbird
from opus_packets import PACKETS, host_player, opus_packets, read_all


class PlayerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.packets = opus_packets()

    def _push_all(self, player: mockingbird.Player, skip: int = -1) -> None:
        for i, p in enumerate(self.packets):
            last = mockingbird.PacketFlag.LAST if i == len(self.packets) - 1 else 0
            if i != skip:
                player.push(p.data, i * 20_000, mockingbird.Codec.OPUS, last)

    def test_plays_every_packet(self) -> None:
        with host_player() as player:
            self._push_all(player)
            out = read_all(player, PACKETS * FRAME)
            read_all(player, 1600)
            st = player.stats()
        self.assertEqual(len(out), PACKETS * FRAME)
        self.assertGreater(max(abs(s) for s in out), 1000)
        self.assertEqual((st.packets_received, st.packets_played), (PACKETS, PACKETS))
        self.assertEqual((st.packets_concealed, st.packets_dropped, st.underruns), (0, 0, 0))
        self.assertIs(st.state, mockingbird.PlayerState.IDLE)

    def test_missing_packet_is_concealed(self) -> None:
        with host_player() as player:
            self._push_all(player, skip=7)
            read_all(player, PACKETS * FRAME + 1600)
            st = player.stats()
        self.assertEqual((st.packets_played, st.packets_concealed), (PACKETS - 1, 1))

    def test_push_packet_and_pcm(self) -> None:
        with host_player() as player:
            player.push_packet(self.packets[0])
            player.push(bytes(FRAME * 2), 20_000, mockingbird.Codec.PCM_S16LE)
            self.assertEqual(player.stats().packets_received, 2)
            player.flush()
            player.set_volume(0.5)
            player.set_muted(True)
            self.assertEqual(len(player.read(FRAME)), FRAME)

    def test_controls_and_errors(self) -> None:
        with host_player() as player:
            with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                player.start()
            self.assertIs(ctx.exception.status, mockingbird.Status.ERR_ALREADY_RUNNING)
            player.pause()
            self.assertIs(player.stats().state, mockingbird.PlayerState.PAUSED)
            player.resume()
            with self.assertRaises(mockingbird.MockingbirdError):
                player.set_volume(1.5)
            player.stop()
            self.assertIs(player.stats().state, mockingbird.PlayerState.STOPPED)
            with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                player.read(FRAME)
            self.assertIs(ctx.exception.status, mockingbird.Status.ERR_NOT_RUNNING)

    def test_invalid_config_raises(self) -> None:
        for bad in ({"sample_rate_hz": 44100}, {"channels": 3}, {"queue_packets": 2}):
            with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                mockingbird.Player(mockingbird.PlayerConfig(**bad))
            self.assertIs(ctx.exception.status, mockingbird.Status.ERR_INVALID_ARG)

    def test_close_twice_and_use_after_close(self) -> None:
        player = host_player()
        player.close()
        player.close()
        with self.assertRaises(mockingbird.MockingbirdError) as ctx:
            player.stats()
        self.assertIs(ctx.exception.status, mockingbird.Status.ERR_INVALID_STATE)
