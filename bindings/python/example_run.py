"""Driving the engine from a file or the microphone, and reporting."""

from __future__ import annotations

import array
import sys
import time
import wave
from pathlib import Path

import auris
from example_config import FIXTURES


def load_labels() -> list[str]:
    """YAMNet's class names, if the label map fixture is present."""
    p = FIXTURES / "yamnet_class_map.txt"
    if not p.exists():
        return []
    return [line.split(" ", 1)[-1] for line in p.read_text().splitlines() if line.strip()]


def print_event(ev: auris.Event, labels: list[str]) -> None:
    match ev:
        case auris.Event(type=auris.EventType.SPEECH_BEGIN):
            print("voice   ▶ speech begin")
        case auris.Event(type=auris.EventType.SPEECH_END):
            print("voice   ■ speech end")
        case auris.Event(type=auris.EventType.ANOMALY, anomaly_reasons=why, confidence=c):
            print(f"anomaly   {why!s} conf={c:.2f}")
        case auris.Event(type=auris.EventType.SOUND_CLASS, sound_class=cls, category=cat):
            name = f" {labels[cls]}" if 0 <= cls < len(labels) else ""
            print(f"sound   [{cat}]{name} (class {cls}, conf {ev.confidence:.2f})")
        case auris.Event(type=auris.EventType.DEGRADATION, degradation_level=lvl):
            print(f"engine  degradation level {lvl} (worker {ev.busy_fraction:.0%} busy)")
        case auris.Event(type=auris.EventType.BACKEND_ERROR):
            print(f"backend error: {ev.backend_error}", file=sys.stderr)


def drain(engine: auris.Engine, labels: list[str]) -> tuple[int, int, int]:
    """Deliver queued events and count packets, bytes and tapped samples."""
    packets = engine.drain_packets()
    for ev in engine.poll_events():
        print_event(ev, labels)
    tapped = len(engine.read_pcm(1 << 15)) // 2 if engine.config.queues.pcm_tap else 0
    return len(packets), sum(len(p.data) for p in packets), tapped


def report(engine: auris.Engine, packets: int, nbytes: int, tapped: int) -> None:
    st, w = engine.stats(), engine.stats().worker
    print(
        f"\n── summary ──\n"
        f"frames captured : {st.frames_captured}\n"
        f"segments        : {st.segments_opened} opened / {st.segments_closed} closed\n"
        f"packets to host : {packets} ({nbytes} bytes encoded)\n"
        f"tapped samples  : {tapped}\n"
        f"input level     : {st.input_rms_dbfs:.1f} dBFS rms, {st.input_peak_dbfs:.1f} peak\n"
        f"worker          : {w.busy_fraction:.2%} busy, max frame {w.max_frame_us} µs, "
        f"{w.wakeups} wakeups\n"
        f"work shed       : YAMNet {w.sound_class_inferences} ran / "
        f"{w.sound_class_inferences_skipped} skipped, RNNoise skipped "
        f"{w.preproc_frames_skipped} quiet frames, degradation level {w.degradation_level}\n"
        f"dropped         : {st.frames_dropped_capture} frames, {st.packets_dropped} packets, "
        f"{st.tap_samples_dropped} tap samples"
    )


def run_file(engine: auris.Engine, path: str, labels: list[str]) -> None:
    with wave.open(path) as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2:
            sys.exit("need 16-bit mono WAV")
        pcm = array.array("h")
        pcm.frombytes(w.readframes(w.getnframes()))
        rate = w.getframerate()
    print(f"processing {Path(path).name} ({len(pcm)} samples @ {rate} Hz)")
    frame = (rate // 1000) * 20
    totals = [0, 0, 0]
    for i in range(len(pcm) // frame):
        engine.push_pcm(pcm[i * frame : (i + 1) * frame], i * 20_000)
        if i % 16 == 15:
            time.sleep(0.015)
            totals = [a + b for a, b in zip(totals, drain(engine, labels))]
    time.sleep(0.3)
    engine.stop()
    totals = [a + b + c for a, b, c in zip(totals, drain(engine, labels), drain(engine, labels))]
    report(engine, *totals)


def run_mic(engine: auris.Engine, seconds: int, labels: list[str]) -> None:
    print(f"listening on the default mic for {seconds}s — Ctrl-C to stop early")
    totals = [0, 0, 0]
    deadline = time.monotonic() + seconds
    try:
        while time.monotonic() < deadline:
            totals = [a + b for a, b in zip(totals, drain(engine, labels))]
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\nstopping…")
    engine.stop()
    report(engine, *totals)
