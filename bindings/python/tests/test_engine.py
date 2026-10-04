"""Lifecycle, passthrough, input coercion and stats."""

import math
import time
import unittest

from helpers import auris, host_config, push, silence, square


class EngineTest(unittest.TestCase):
    def test_invalid_config_raises_with_status(self) -> None:
        with self.assertRaises(auris.AurisError) as ctx:
            auris.Engine(host_config(sample_rate_hz=12345))
        self.assertIs(ctx.exception.status, auris.Status.ERR_INVALID_ARG)

    def test_context_manager_and_use_after_close(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            self.assertTrue(e.is_running)
        e.close()
        with self.assertRaises(auris.AurisError):
            e.stats()

    def test_passthrough_roundtrip(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            push(e, silence(), 10)
            time.sleep(0.15)
            packets = e.drain_packets()
            st = e.stats()
        self.assertEqual(len(packets), 10)
        self.assertEqual(sum(len(p.data) for p in packets), 10 * 320 * 2)
        self.assertTrue(all(p.codec is auris.Codec.PCM_S16LE for p in packets))
        self.assertEqual((st.frames_captured, st.frames_dropped_capture), (3200, 0))

    def test_push_pcm_accepts_bytes_lists_and_views(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            e.push_pcm(silence().tobytes())
            e.push_pcm([0] * 320)
            e.push_pcm(memoryview(silence()))
            time.sleep(0.12)
            self.assertEqual(e.stats().frames_captured, 3 * 320)

    def test_stats_report_input_level(self) -> None:
        amp = 8192
        with auris.Engine(host_config()) as e:
            e.start()
            push(e, square(320, amp), 10)
            time.sleep(0.15)
            st = e.stats()
        want = 20 * math.log10(amp / 32768.0)
        self.assertAlmostEqual(st.input_rms_dbfs, want, delta=0.1)
        self.assertAlmostEqual(st.input_peak_dbfs, want, delta=0.1)
        self.assertGreaterEqual(st.worker.busy_fraction, 0.0)
