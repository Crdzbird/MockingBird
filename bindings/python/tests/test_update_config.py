"""Live retuning."""

import unittest

from helpers import mockingbird, host_config


class UpdateConfigTest(unittest.TestCase):
    def test_tunables_apply_and_structural_is_rejected(self) -> None:
        cfg = host_config()
        cfg.anomaly.enabled = True
        with mockingbird.Engine(cfg) as e:
            e.start()
            cfg.anomaly.rms_z = 1.75
            cfg.vad.aggressiveness = 0
            e.update_config(cfg)
            self.assertAlmostEqual(e.config.anomaly.rms_z, 1.75, places=4)
            with self.assertRaises(mockingbird.MockingbirdError):
                e.update_config(host_config(sample_rate_hz=48000))

    def test_cannot_enable_a_detector_that_was_off(self) -> None:
        cfg = host_config()
        with mockingbird.Engine(cfg) as e:
            e.start()
            cfg.anomaly.enabled = True
            with self.assertRaises(mockingbird.MockingbirdError):
                e.update_config(cfg)
