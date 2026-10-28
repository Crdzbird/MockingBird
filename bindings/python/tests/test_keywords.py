"""A keyword model fires once above its threshold on the speech fixture."""

import unittest

from helpers import FIXTURES, FRAME, mockingbird, host_config, read_wav

ASSETS = FIXTURES.parents[1] / "models"


@unittest.skipUnless((FIXTURES / "keyword_test.onnx").exists(), "fixture unavailable")
class KeywordTest(unittest.TestCase):
    def test_keyword_fires_once(self) -> None:
        pcm, _ = read_wav(FIXTURES / "hello.wav")
        cfg = host_config(queues=mockingbird.QueueConfig(pcm_ring_frames=1 << 18))
        with mockingbird.Engine(cfg) as e:
            e.set_keyword_frontend(ASSETS / "melspectrogram.onnx", ASSETS / "embedding_model.onnx")
            self.assertEqual(e.add_keyword(FIXTURES / "keyword_test.onnx", 0.9), 0)
            e.start()
            with self.assertRaises(mockingbird.MockingbirdError):
                e.add_keyword(FIXTURES / "keyword_test.onnx")
            for i in range(len(pcm) // FRAME):
                e.push_pcm(pcm[i * FRAME : (i + 1) * FRAME], i * 20_000)
            e.stop()
            events = e.poll_events()
            e.drain_packets()
        if any(ev.type is mockingbird.EventType.BACKEND_ERROR for ev in events):
            self.skipTest("this build has no ONNX Runtime")
        hits = [ev for ev in events if ev.type is mockingbird.EventType.KEYWORD]
        self.assertEqual(len(hits), 1)
        self.assertEqual(hits[0].reason_mask, 0)
        self.assertGreaterEqual(hits[0].confidence, 0.9)
