"""The Opus decoder against the engine's own packets."""

import unittest

from helpers import FRAME, mockingbird
from opus_packets import PACKETS, opus_packets


class DecoderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.packets = opus_packets()

    def test_engine_packets_round_trip(self) -> None:
        self.assertEqual(len(self.packets), PACKETS)
        self.assertTrue(all(p.codec is mockingbird.Codec.OPUS for p in self.packets))
        with mockingbird.Decoder(16000, 1) as d:
            pcm = [s for p in self.packets for s in d.decode(p.data)]
        self.assertEqual(len(pcm), PACKETS * FRAME)
        self.assertGreater(max(abs(s) for s in pcm), 1000)

    def test_decode_accepts_buffer_objects(self) -> None:
        with mockingbird.Decoder() as d:
            self.assertEqual(len(d.decode(bytearray(self.packets[0].data))), FRAME)
            self.assertEqual(len(d.decode(memoryview(self.packets[1].data))), FRAME)

    def test_stereo_output_is_interleaved(self) -> None:
        with mockingbird.Decoder(48000, 2) as d:
            self.assertEqual(len(d.decode(self.packets[0].data)), 960 * 2)
            self.assertEqual(len(d.conceal()), 960 * 2)

    def test_conceal_length(self) -> None:
        with mockingbird.Decoder() as d:
            self.assertEqual(len(d.conceal()), FRAME)
            d.decode(self.packets[0].data)
            self.assertEqual(len(d.conceal()), FRAME)
            self.assertEqual(len(d.conceal(160)), 160)
            d.reset()
            self.assertEqual(len(d.decode(self.packets[1].data)), FRAME)

    def test_garbage_packet_raises_and_decoder_stays_usable(self) -> None:
        with mockingbird.Decoder() as d:
            for bad in (b"\xff\xff\xff", b""):
                with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                    d.decode(bad)
                self.assertIs(ctx.exception.status, mockingbird.Status.ERR_INVALID_ARG)
            with self.assertRaises(mockingbird.MockingbirdError):
                d.conceal(7)
            self.assertEqual(len(d.decode(self.packets[0].data)), FRAME)

    def test_invalid_format_raises(self) -> None:
        for rate, channels in ((44100, 1), (16000, 3)):
            with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                mockingbird.Decoder(rate, channels)
            self.assertIs(ctx.exception.status, mockingbird.Status.ERR_INVALID_ARG)

    def test_close_twice_and_use_after_close(self) -> None:
        d = mockingbird.Decoder()
        d.close()
        d.close()
        with self.assertRaises(mockingbird.MockingbirdError) as ctx:
            d.decode(self.packets[0].data)
        self.assertIs(ctx.exception.status, mockingbird.Status.ERR_INVALID_STATE)
