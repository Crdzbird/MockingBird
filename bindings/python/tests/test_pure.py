"""No engine required: categories, the sensitivity dial, defaults."""

import unittest

from helpers import mockingbird


class SoundCategoryTest(unittest.TestCase):
    def test_ranges(self) -> None:
        for c in (0, 2, 3, 12):
            self.assertIs(mockingbird.SoundCategory.of(c), mockingbird.SoundCategory.SPEECH)
        for c in (67, 70, 131):
            self.assertIs(mockingbird.SoundCategory.of(c), mockingbird.SoundCategory.ANIMAL)
        for c in (132, 200, 282):
            self.assertIs(mockingbird.SoundCategory.of(c), mockingbird.SoundCategory.MUSIC)
        for c in (283, 299):
            self.assertIs(mockingbird.SoundCategory.of(c), mockingbird.SoundCategory.NATURE)
        for c in (494, 300, -1, 99999):
            self.assertIs(mockingbird.SoundCategory.of(c), mockingbird.SoundCategory.OTHER)

    def test_human_voice(self) -> None:
        for c in (0, 2, 12, 13, 24, 34):
            self.assertTrue(mockingbird.SoundCategory.is_human_voice(c))
        for c in (35, 48, 67, 132, 494, -1):
            self.assertFalse(mockingbird.SoundCategory.is_human_voice(c))


class SensitiveTest(unittest.TestCase):
    def test_endpoints(self) -> None:
        low, high = mockingbird.sensitive(0.0), mockingbird.sensitive(1.0)
        self.assertAlmostEqual(low.vad.neural_threshold, 0.80, places=4)
        self.assertAlmostEqual(high.vad.neural_threshold, 0.20, places=4)
        self.assertEqual((low.vad.aggressiveness, high.vad.aggressiveness), (3, 0))
        self.assertAlmostEqual(low.anomaly.rms_z, 5.0, places=4)
        self.assertAlmostEqual(high.anomaly.rms_z, 1.5, places=4)
        self.assertAlmostEqual(low.sound_class.threshold, 0.70, places=4)
        self.assertAlmostEqual(high.sound_class.threshold, 0.12, places=4)

    def test_clamped_and_defaults(self) -> None:
        self.assertEqual(mockingbird.Sensitivity(5).value, 1.0)
        self.assertEqual(mockingbird.Sensitivity(-5).value, 0.0)
        mid = mockingbird.sensitive(0.5)
        self.assertIs(mid.vad.tier, mockingbird.VadTier.NEURAL)
        self.assertTrue(mid.vad.enabled and mid.anomaly.enabled and mid.sound_class.enabled)

    def test_keyword_groups(self) -> None:
        cfg = mockingbird.sensitive(0.5, denoise=True, encoder=mockingbird.EncoderConfig.opus())
        self.assertTrue(cfg.denoise)
        self.assertIs(cfg.encoder.codec, mockingbird.Codec.OPUS)


class DefaultsTest(unittest.TestCase):
    def test_match_the_native_defaults(self) -> None:
        cfg = mockingbird.Config()
        self.assertEqual(cfg.performance.silence_gate_dbfs, -60.0)
        self.assertFalse(cfg.performance.adaptive_degradation)
        self.assertEqual(cfg.performance.degrade_busy_ppm, 700_000)
        self.assertEqual(cfg.sound_class.interval_ms, 1000)
        self.assertEqual(mockingbird.PerformanceConfig.mobile().worker_batch_frames, 4)

    def test_version_and_status(self) -> None:
        self.assertRegex(mockingbird.version(), r"^\d+\.\d+\.\d+")
        self.assertEqual(mockingbird.Status.of(-7), mockingbird.Status.ERR_QUEUE_EMPTY)
        self.assertEqual(mockingbird.Status.of(99), 99)
