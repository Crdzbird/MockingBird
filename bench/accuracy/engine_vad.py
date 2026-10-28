"""Speech intervals from the Mockingbird engine itself, through the Python
binding: SPEECH_BEGIN and SPEECH_END events on pushed PCM. SPEECH_END is
stamped when the hangover confirms the end, so an interval ends where the
hangover began, which is how the other packages report the end of speech."""

import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "bindings" / "python"))

import mockingbird  # noqa: E402

FRAME = 320


def intervals(signal: np.ndarray, neural: bool, hangover_ms: int | None = None) -> list[tuple[float, float]]:
    cfg = mockingbird.Config(backend=mockingbird.Backend.HOST_PCM)
    if hangover_ms is not None:
        cfg.vad.hangover_ms = hangover_ms
    cfg.anomaly.enabled = False
    cfg.queues.pcm_ring_frames = 1 << 20
    cfg.queues.packet_queue_capacity = 8192
    if neural:
        cfg.vad.tier = mockingbird.VadTier.NEURAL
        cfg.vad.model_path = str(ROOT / "tests" / "fixtures" / "silero_vad.onnx")
    pcm = (np.clip(signal, -1, 1) * 32767).astype(np.int16)
    out, start = [], None
    with mockingbird.Engine(cfg) as e:
        e.start()
        for i in range(0, len(pcm) - FRAME + 1, FRAME):
            e.push_pcm(pcm[i : i + FRAME], i * 1_000_000 // 16000)
            if i % (FRAME * 64) == 0:
                e.drain_packets()
                while e.stats().pcm_ring_fill_frames > FRAME * 256:
                    time.sleep(0.001)
        e.stop()
        dropped = e.stats().frames_dropped_capture
        if dropped:
            raise RuntimeError(f"the engine dropped {dropped} samples")
        for ev in e.poll_events():
            if ev.type == mockingbird.EventType.SPEECH_BEGIN:
                start = ev.timestamp_us / 1e6
            elif ev.type == mockingbird.EventType.SPEECH_END and start is not None:
                end = ev.timestamp_us / 1e6 - cfg.vad.hangover_ms / 1000
                out.append((start, max(start, end)))
                start = None
        if start is not None:
            out.append((start, len(pcm) / 16000))
    return out
