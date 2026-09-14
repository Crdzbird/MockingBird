"""Test suite for the Auris Python binding.

Uses stdlib unittest so it runs with no extra dependencies:

    python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import array
import math
import os
import sys
import time
import unittest
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import auris  # noqa: E402

FIXTURES = Path(__file__).resolve().parents[3] / "tests" / "fixtures"


def host_config(**overrides: object) -> auris.Config:
    cfg = auris.Config(backend=auris.Backend.HOST_PCM, codec=auris.Codec.PCM_S16LE)
    for k, v in overrides.items():
        setattr(cfg, k, v)
    return cfg


def silence(n: int = 320) -> array.array:
    return array.array("h", [0] * n)


def square(n: int, amp: int) -> array.array:
    return array.array("h", [amp if i % 2 == 0 else -amp for i in range(n)])


def read_wav(path: Path) -> tuple[array.array, int]:
    with wave.open(str(path)) as w:
        pcm = array.array("h")
        pcm.frombytes(w.readframes(w.getnframes()))
        return pcm, w.getframerate()


class TestPureLogic(unittest.TestCase):
    """No engine required — mirrors the Go/Dart category + dial tests."""

    def test_version(self) -> None:
        self.assertRegex(auris.version(), r"^\d+\.\d+\.\d+")

    def test_category_mapping(self) -> None:
        # The speech cluster includes Conversation(2)/Narration(3), where
        # non-English speech commonly lands.
        for c in (0, 1, 2, 3, 4, 6, 9, 12):
            self.assertIs(auris.category_for_yamnet_class(c), auris.SoundCategory.SPEECH)
            self.assertTrue(auris.is_speech_class(c))
        for c in (67, 70, 131):
            self.assertIs(auris.category_for_yamnet_class(c), auris.SoundCategory.ANIMAL)
        for c in (132, 200, 282):
            self.assertIs(auris.category_for_yamnet_class(c), auris.SoundCategory.MUSIC)
        for c in (283, 299):
            self.assertIs(auris.category_for_yamnet_class(c), auris.SoundCategory.NATURE)
        for c in (494, 300, -1, 99999):
            self.assertIs(auris.category_for_yamnet_class(c), auris.SoundCategory.OTHER)

    def test_is_human_voice(self) -> None:
        for c in (0, 2, 12, 13, 24, 34):
            self.assertTrue(auris.is_human_voice(c))
        for c in (35, 48, 67, 132, 494, -1):
            self.assertFalse(auris.is_human_voice(c))

    def test_sensitive_dial(self) -> None:
        low, high = auris.sensitive(0.0), auris.sensitive(1.0)
        self.assertGreater(low.vad_neural_threshold, high.vad_neural_threshold)
        self.assertAlmostEqual(low.vad_neural_threshold, 0.80, places=4)
        self.assertAlmostEqual(high.vad_neural_threshold, 0.20, places=4)
        self.assertEqual(low.vad_aggressiveness, 3)
        self.assertEqual(high.vad_aggressiveness, 0)
        self.assertAlmostEqual(low.anomaly_rms_z, 5.0, places=4)
        self.assertAlmostEqual(high.anomaly_rms_z, 1.5, places=4)
        # Clamped, and all detectors on with neural VAD by default.
        self.assertEqual(auris.sensitive(5.0).vad_neural_threshold, high.vad_neural_threshold)
        self.assertEqual(auris.sensitive(-5.0).vad_neural_threshold, low.vad_neural_threshold)
        mid = auris.sensitive(0.5)
        self.assertIs(mid.vad_tier, auris.VadTier.NEURAL)
        self.assertTrue(mid.enable_vad and mid.enable_anomaly and mid.enable_sound_class)

    def test_sensitive_overrides(self) -> None:
        cfg = auris.sensitive(0.5, enable_rnnoise=True, codec=auris.Codec.OPUS)
        self.assertTrue(cfg.enable_rnnoise)
        self.assertIs(cfg.codec, auris.Codec.OPUS)
        with self.assertRaises(TypeError):
            auris.sensitive(0.5, not_a_field=1)


class TestEngineBasics(unittest.TestCase):
    def test_invalid_config_raises_with_status(self) -> None:
        cfg = host_config(sample_rate_hz=12345)
        with self.assertRaises(auris.AurisError) as ctx:
            auris.Engine(cfg)
        self.assertIs(ctx.exception.status, auris.Status.ERR_INVALID_ARG)

    def test_context_manager_and_idempotent_close(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            self.assertTrue(e.is_running)
        # closed by __exit__; closing again is a no-op, and use-after-close raises
        with self.assertRaises(auris.AurisError):
            e.stats()

    def test_passthrough_roundtrip(self) -> None:
        cfg = host_config(enable_vad=False, enable_anomaly=False)
        with auris.Engine(cfg) as e:
            e.start()
            for i in range(10):
                e.push_pcm(silence(), i * 20_000)
            time.sleep(0.15)
            packets = e.drain_packets()
            st = e.stats()
        self.assertEqual(len(packets), 10)
        self.assertEqual(sum(len(p.data) for p in packets), 10 * 320 * 2)
        self.assertEqual(st.frames_captured, 3200)
        self.assertEqual(st.frames_dropped_capture, 0)

    def test_push_pcm_accepts_bytes_and_lists(self) -> None:
        cfg = host_config(enable_vad=False, enable_anomaly=False)
        with auris.Engine(cfg) as e:
            e.start()
            e.push_pcm(silence().tobytes())        # buffer protocol
            e.push_pcm([0] * 320)                  # plain sequence
            e.push_pcm(memoryview(silence()))      # memoryview
            time.sleep(0.12)
            st = e.stats()
        self.assertEqual(st.frames_captured, 3 * 320)

    def test_stats_report_input_level(self) -> None:
        amp = 8192
        with auris.Engine(host_config(enable_vad=False, enable_anomaly=False)) as e:
            e.start()
            for i in range(10):
                e.push_pcm(square(320, amp), i * 20_000)
            time.sleep(0.15)
            st = e.stats()
        want = 20 * math.log10(amp / 32768.0)
        self.assertAlmostEqual(st.input_rms_dbfs, want, delta=0.1)
        self.assertAlmostEqual(st.input_peak_dbfs, want, delta=0.1)


class TestPcmTap(unittest.TestCase):
    def test_tap_delivers_ungated_stream(self) -> None:
        # VAD on + silence => no segment, so no packets; the tap still flows.
        cfg = host_config(enable_vad=True, enable_anomaly=False,
                          enable_pcm_tap=True, pcm_tap_frames=1 << 15)
        with auris.Engine(cfg) as e:
            e.start()
            for i in range(15):
                e.push_pcm(silence(), i * 20_000)
            time.sleep(0.2)
            packets = e.drain_packets()
            tapped = b""
            while chunk := e.read_pcm(4096):
                tapped += chunk
            st = e.stats()
        self.assertEqual(len(packets), 0)          # nothing triggered a segment
        self.assertEqual(len(tapped) // 2, 15 * 320)  # ...but every sample tapped
        self.assertEqual(st.tap_samples_dropped, 0)

    def test_read_pcm_without_tap_raises(self) -> None:
        with auris.Engine(host_config()) as e:
            e.start()
            with self.assertRaises(auris.AurisError) as ctx:
                e.read_pcm()
        self.assertIs(ctx.exception.status, auris.Status.ERR_UNSUPPORTED)


class TestUpdateConfig(unittest.TestCase):
    def test_tunables_apply_and_structural_rejected(self) -> None:
        cfg = host_config()
        with auris.Engine(cfg) as e:
            e.start()
            cfg.anomaly_rms_z = 1.75
            cfg.vad_aggressiveness = 0
            e.update_config(cfg)          # tunable: fine
            self.assertAlmostEqual(e.config.anomaly_rms_z, 1.75, places=4)

            bad = host_config(sample_rate_hz=48000)
            with self.assertRaises(auris.AurisError):
                e.update_config(bad)      # structural: rejected

    def test_cannot_enable_detector_that_was_off(self) -> None:
        cfg = host_config(enable_anomaly=False)
        with auris.Engine(cfg) as e:
            e.start()
            cfg.enable_anomaly = True
            with self.assertRaises(auris.AurisError):
                e.update_config(cfg)


@unittest.skipUnless((FIXTURES / "hello.wav").exists(), "fixture unavailable")
class TestRealVoice(unittest.TestCase):
    def test_detects_speech_in_fixture(self) -> None:
        pcm, rate = read_wav(FIXTURES / "hello.wav")
        self.assertEqual(rate, 16000)
        cfg = host_config(enable_vad=True, vad_tier=auris.VadTier.FAST,
                          vad_aggressiveness=1, enable_anomaly=False,
                          pcm_ring_frames=1 << 17)
        speech_begins = 0
        frame = 320
        with auris.Engine(cfg) as e:
            e.start()
            for i in range(len(pcm) // frame):
                e.push_pcm(pcm[i * frame:(i + 1) * frame], i * 20_000)
                if i % 16 == 15:
                    time.sleep(0.02)
                    speech_begins += sum(
                        1 for ev in e.poll_events()
                        if ev.type is auris.EventType.SPEECH_BEGIN
                    )
                    e.drain_packets()
            time.sleep(0.3)
            speech_begins += sum(
                1 for ev in e.poll_events()
                if ev.type is auris.EventType.SPEECH_BEGIN
            )
        self.assertGreaterEqual(speech_begins, 1)



class SilenceGateAndHealthTest(unittest.TestCase):
    """Silence gate + worker-health stats through the Python surface."""

    def test_defaults(self) -> None:
        cfg = auris.Config()
        self.assertEqual(cfg.silence_gate_dbfs, -60.0)
        self.assertFalse(cfg.enable_adaptive_degradation)
        self.assertEqual(cfg.degrade_busy_ppm, 700_000)
        self.assertEqual(cfg.sound_class_interval_ms, 1000)
        self.assertEqual(auris.EventType.DEGRADATION, 9)

    def test_rnnoise_gate_and_health(self) -> None:
        cfg = host_config(enable_vad=False, enable_anomaly=False, enable_rnnoise=True)
        try:
            engine = auris.Engine(cfg)
        except Exception as exc:  # library built without RNNoise
            self.skipTest(f"RNNoise not compiled in: {exc}")
        with engine:
            engine.start()
            silent = array.array("h", [0] * 320)
            loud = array.array("h", [3000 if i % 32 < 16 else -3000 for i in range(320)])
            ts = 0
            for _ in range(25):
                engine.push_pcm(silent, ts); ts += 20_000
            time.sleep(0.15)
            st = engine.stats()
            self.assertEqual(st.preproc_frames_skipped, 25)
            self.assertEqual(st.degradation_level, 0)
            for _ in range(25):
                engine.push_pcm(loud, ts); ts += 20_000
            time.sleep(0.15)
            st = engine.stats()
            self.assertEqual(st.preproc_frames_skipped, 25)
            self.assertGreaterEqual(st.worker_busy_fraction, 0.0)
            self.assertLessEqual(st.worker_busy_fraction, 1.0)
            # Live retune: disabling the gate stops the skipping.
            cfg.silence_gate_dbfs = -120.0
            engine.update_config(cfg)
            for _ in range(10):
                engine.push_pcm(silent, ts); ts += 20_000
            time.sleep(0.15)
            self.assertEqual(engine.stats().preproc_frames_skipped, 25)

if __name__ == "__main__":
    unittest.main()
