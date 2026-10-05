"""A recorded utterance must produce at least one speech onset."""

import time
import unittest

from helpers import FIXTURES, FRAME, mockingbird, host_config, read_wav


@unittest.skipUnless((FIXTURES / "hello.wav").exists(), "fixture unavailable")
class RealVoiceTest(unittest.TestCase):
    def test_fast_vad_detects_speech(self) -> None:
        pcm, rate = read_wav(FIXTURES / "hello.wav")
        self.assertEqual(rate, 16000)
        cfg = host_config(queues=mockingbird.QueueConfig(pcm_ring_frames=1 << 17))
        cfg.vad = mockingbird.VadConfig(tier=mockingbird.VadTier.FAST, aggressiveness=1)
        onsets = 0

        def count() -> int:
            return sum(ev.type is mockingbird.EventType.SPEECH_BEGIN for ev in e.poll_events())

        with mockingbird.Engine(cfg) as e:
            e.start()
            for i in range(len(pcm) // FRAME):
                e.push_pcm(pcm[i * FRAME : (i + 1) * FRAME], i * 20_000)
                if i % 16 == 15:
                    time.sleep(0.02)
                    onsets += count()
                    e.drain_packets()
            time.sleep(0.3)
            onsets += count()
        self.assertGreaterEqual(onsets, 1)
