#!/usr/bin/env python3
"""A flag-driven tour of the Auris engine from Python.

Runs any combination of backend (live mic or a WAV file via HOST_PCM), VAD tier,
anomaly gate, YAMNet classification, RNNoise, and codec — plus named presets.

    python3 example.py                                  # default mic
    python3 example.py --preset everything --file ../../tests/fixtures/hello.wav
    python3 example.py --preset music --file ../../tests/fixtures/miaow_16k.wav
    python3 example.py --seconds 10 --sensitivity 0.8   # live mic, eager
"""

from __future__ import annotations

import argparse
import array
import sys
import time
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import auris

FIXTURES = Path(__file__).resolve().parents[2] / "tests" / "fixtures"


def locate(name: str) -> str | None:
    p = FIXTURES / name
    return str(p) if p.exists() else None


def load_labels() -> list[str]:
    p = FIXTURES / "yamnet_class_map.txt"
    if not p.exists():
        return []
    out = []
    for line in p.read_text().splitlines():
        line = line.strip()
        if line:
            out.append(line.split(" ", 1)[-1])
    return out


def build_config(args: argparse.Namespace) -> auris.Config:
    silero, yamnet = locate("silero_vad.onnx"), locate("yamnet_3s.onnx")
    presets = {
        "voice": dict(enable_anomaly=False, enable_sound_class=False,
                      enable_rnnoise=True, codec=auris.Codec.OPUS),
        "everything": dict(enable_rnnoise=True, codec=auris.Codec.OPUS),
        "anomaly": dict(enable_vad=False, enable_sound_class=False,
                        anomaly_rms_z=2.0, anomaly_flux_z=2.0),
        "music": dict(enable_vad=False, enable_anomaly=False,
                      sound_class_threshold=0.3),
    }
    overrides: dict[str, object] = dict(
        neural_vad_model_path=silero, sound_class_model_path=yamnet
    )
    overrides.update(presets.get(args.preset, {}))
    cfg = auris.sensitive(args.sensitivity, **overrides)

    if args.file:
        cfg.backend = auris.Backend.HOST_PCM
        cfg.pcm_ring_frames = 1 << 17
        cfg.packet_queue_capacity = 4096
    if args.tap:
        cfg.enable_pcm_tap = True
        cfg.pcm_tap_frames = 1 << 16

    # Degrade gracefully instead of failing when a model or build flag is absent.
    if cfg.enable_vad and cfg.vad_tier is auris.VadTier.NEURAL and not silero:
        print("note: no silero_vad.onnx — falling back to fast VAD", file=sys.stderr)
        cfg.vad_tier = auris.VadTier.FAST
    if cfg.enable_sound_class and not yamnet:
        print("note: no yamnet_3s.onnx — disabling sound classification", file=sys.stderr)
        cfg.enable_sound_class = False
    return cfg


def describe(cfg: auris.Config, preset: str) -> str:
    on = []
    if cfg.enable_vad:
        on.append("VAD:" + ("neural" if cfg.vad_tier is auris.VadTier.NEURAL else "fast"))
    if cfg.enable_anomaly:
        on.append("anomaly")
    if cfg.enable_sound_class:
        on.append("YAMNet")
    if cfg.enable_rnnoise:
        on.append("RNNoise")
    if cfg.enable_pcm_tap:
        on.append("tap")
    tag = f"preset={preset} · " if preset else ""
    backend = "host-pcm" if cfg.backend is auris.Backend.HOST_PCM else "mic"
    return f"{tag}{backend} · {cfg.sample_rate_hz} Hz · {cfg.codec.name.lower()} · [{' '.join(on)}]"


def print_event(ev: auris.Event, labels: list[str]) -> None:
    if ev.type is auris.EventType.SPEECH_BEGIN:
        print("voice   ▶ speech begin")
    elif ev.type is auris.EventType.SPEECH_END:
        print("voice   ■ speech end")
    elif ev.type is auris.EventType.ANOMALY:
        print(f"anomaly   reason={ev.reason_mask} conf={ev.confidence:.2f}")
    elif ev.type is auris.EventType.SOUND_CLASS:
        cls = ev.sound_class
        name = f" {labels[cls]}" if 0 <= cls < len(labels) else ""
        cat = auris.category_for_yamnet_class(cls)
        print(f"sound   [{cat}]{name} (class {cls}, conf {ev.confidence:.2f})")
    elif ev.type is auris.EventType.BACKEND_ERROR:
        print(f"backend error: {ev.backend_error}", file=sys.stderr)


def report(engine: auris.Engine, packets: int, nbytes: int, tapped: int) -> None:
    st = engine.stats()
    print(
        f"\n── summary ──\n"
        f"frames captured : {st.frames_captured}\n"
        f"segments        : {st.segments_opened} opened / {st.segments_closed} closed\n"
        f"packets to host : {packets} ({nbytes} bytes encoded)\n"
        f"tapped samples  : {tapped}\n"
        f"input level     : {st.input_rms_dbfs:.1f} dBFS rms, {st.input_peak_dbfs:.1f} peak\n"
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
    packets = nbytes = tapped = 0

    def drain() -> tuple[int, int, int]:
        p = engine.drain_packets()
        for ev in engine.poll_events():
            print_event(ev, labels)
        tap = len(engine.read_pcm(1 << 15)) // 2 if engine.config.enable_pcm_tap else 0
        return len(p), sum(len(x.data) for x in p), tap

    for i in range(len(pcm) // frame):
        engine.push_pcm(pcm[i * frame:(i + 1) * frame], i * 20_000)
        if i % 16 == 15:
            time.sleep(0.015)
            a, b, c = drain()
            packets += a
            nbytes += b
            tapped += c
    time.sleep(0.3)
    a, b, c = drain()
    engine.stop()
    a2, b2, c2 = drain()
    report(engine, packets + a + a2, nbytes + b + b2, tapped + c + c2)


def run_mic(engine: auris.Engine, seconds: int, labels: list[str]) -> None:
    print(f"listening on the default mic for {seconds}s — Ctrl-C to stop early")
    packets = nbytes = tapped = 0
    deadline = time.monotonic() + seconds
    try:
        while time.monotonic() < deadline:
            for ev in engine.poll_events():
                print_event(ev, labels)
            p = engine.drain_packets()
            packets += len(p)
            nbytes += sum(len(x.data) for x in p)
            if engine.config.enable_pcm_tap:
                tapped += len(engine.read_pcm(1 << 15)) // 2
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\nstopping…")
    engine.stop()
    report(engine, packets, nbytes, tapped)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preset", default="", choices=["", "voice", "everything", "anomaly", "music"])
    ap.add_argument("--file", help="16-bit mono WAV to process via HOST_PCM")
    ap.add_argument("--sensitivity", type=float, default=0.6, help="0..1 detector dial")
    ap.add_argument("--seconds", type=int, default=15, help="mic capture duration")
    ap.add_argument("--tap", action="store_true", help="enable the continuous PCM tap")
    args = ap.parse_args()

    cfg = build_config(args)
    labels = load_labels()
    print(f"Auris {auris.version()} — {describe(cfg, args.preset)}")

    with auris.Engine(cfg) as engine:
        engine.start()
        if args.file:
            run_file(engine, args.file, labels)
        else:
            run_mic(engine, args.seconds, labels)


if __name__ == "__main__":
    main()
