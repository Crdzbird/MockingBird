"""The silence gate and worker health, observed through RNNoise."""

import time
import unittest

from helpers import auris, host_config, push, silence, square


class SilenceGateTest(unittest.TestCase):
    def test_rnnoise_skips_quiet_frames_until_the_gate_is_disabled(self) -> None:
        cfg = host_config(denoise=True)
        try:
            engine = auris.Engine(cfg)
        except auris.AurisError as exc:
            self.skipTest(f"RNNoise not compiled in: {exc}")
        loud = square(320, 3000, period=32)
        with engine:
            engine.start()
            ts = push(engine, silence(), 25)
            time.sleep(0.15)
            st = engine.stats()
            self.assertEqual(st.worker.preproc_frames_skipped, 25)
            self.assertEqual(st.worker.degradation_level, 0)
            ts = push(engine, loud, 25, ts)
            time.sleep(0.15)
            self.assertEqual(engine.stats().worker.preproc_frames_skipped, 25)
            cfg.performance.silence_gate_dbfs = -120.0
            engine.update_config(cfg)
            push(engine, silence(), 10, ts)
            time.sleep(0.15)
            self.assertEqual(engine.stats().worker.preproc_frames_skipped, 25)
