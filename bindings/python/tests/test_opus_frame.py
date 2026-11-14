"""Opus has no 30 ms frame, so the engine refuses it at creation."""

import unittest

from helpers import mockingbird, host_config


class OpusFrameTest(unittest.TestCase):
    def test_opus_and_ogg_opus_reject_30_ms_frames(self) -> None:
        for encoder in (mockingbird.EncoderConfig.opus(), mockingbird.EncoderConfig.ogg_opus()):
            with self.subTest(codec=encoder.codec):
                with self.assertRaises(mockingbird.MockingbirdError) as ctx:
                    mockingbird.Engine(host_config(frame_ms=30, encoder=encoder))
                self.assertIs(ctx.exception.status, mockingbird.Status.ERR_UNSUPPORTED)

    def test_pcm_keeps_30_ms_frames(self) -> None:
        with mockingbird.Engine(host_config(frame_ms=30)) as e:
            e.start()
            self.assertTrue(e.is_running)
