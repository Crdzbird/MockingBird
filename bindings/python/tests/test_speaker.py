"""Speaker verification tells the two fixture speakers apart."""

import array
import unittest

from helpers import FIXTURES, FRAME, mockingbird, host_config, read_wav

MODEL = FIXTURES.parents[1] / "models" / "speaker_campplus.onnx"


@unittest.skipUnless(MODEL.exists(), "model unavailable")
class SpeakerTest(unittest.TestCase):
    def test_target_and_other_speaker(self) -> None:
        a1, _ = read_wav(FIXTURES / "speaker_a1.wav")
        a2, _ = read_wav(FIXTURES / "speaker_a2.wav")
        b, _ = read_wav(FIXTURES / "speaker_b.wav")
        cfg = host_config(queues=mockingbird.QueueConfig(pcm_ring_frames=1 << 20))
        cfg.vad = mockingbird.VadConfig(tier=mockingbird.VadTier.FAST, aggressiveness=3)
        audio = array.array("h", [0] * 16000) + a2 + array.array("h", [0] * 32000) + b + array.array("h", [0] * 32000)
        with mockingbird.Engine(cfg) as e:
            e.set_speaker_model(MODEL)
            try:
                target = e.speaker_embedding(a1)
            except mockingbird.MockingbirdError:
                self.skipTest("this build has no ONNX Runtime")
            self.assertEqual(len(target), 192)
            e.set_target_speaker(target)
            e.start()
            for i in range(len(audio) // FRAME):
                e.push_pcm(audio[i * FRAME : (i + 1) * FRAME], i * 20_000)
            e.stop()
            verdicts = [ev for ev in e.poll_events() if ev.type is mockingbird.EventType.SPEAKER]
            e.drain_packets()
        self.assertGreaterEqual(len(verdicts), 2)
        self.assertEqual(verdicts[0].reason_mask, 1)
        self.assertEqual(verdicts[-1].reason_mask, 0)
