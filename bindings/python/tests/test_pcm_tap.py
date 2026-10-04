"""The ungated PCM tap."""

import time
import unittest

from helpers import auris, host_config, push, silence


class PcmTapTest(unittest.TestCase):
    def test_tap_delivers_every_frame_without_a_segment(self) -> None:
        cfg = host_config(queues=auris.QueueConfig(pcm_tap=True, pcm_tap_frames=1 << 15))
        cfg.vad.enabled = True
        with auris.Engine(cfg) as e:
            e.start()
            push(e, silence(), 15)
            time.sleep(0.2)
            packets = e.drain_packets()
            tapped = b"".join(iter(lambda: e.read_pcm(4096), b""))
            st = e.stats()
        self.assertEqual(len(packets), 0)
        self.assertEqual(len(tapped) // 2, 15 * 320)
        self.assertEqual(st.tap_samples_dropped, 0)

    def test_read_pcm_without_tap_raises(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            with self.assertRaises(auris.AurisError) as ctx:
                e.read_pcm()
        self.assertIs(ctx.exception.status, auris.Status.ERR_UNSUPPORTED)
