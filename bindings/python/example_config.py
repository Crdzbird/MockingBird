"""Building the demo's engine configuration from presets and flags."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import mockingbird

FIXTURES = Path(__file__).resolve().parents[2] / "tests" / "fixtures"


def locate(name: str) -> str | None:
    """The fixture path for ``name`` if it exists."""
    p = FIXTURES / name
    return str(p) if p.exists() else None


def build_config(args: argparse.Namespace) -> mockingbird.Config:
    """One sensitivity dial, a preset, and the file-versus-mic adjustments."""
    silero, yamnet = locate("silero_vad.onnx"), locate("yamnet_3s.onnx")
    live = not args.file
    cfg = mockingbird.sensitive(
        args.sensitivity,
        backend=mockingbird.Backend.AUTO if live else mockingbird.Backend.HOST_PCM,
        denoise=args.preset in ("voice", "everything"),
        vad=args.preset not in ("anomaly", "music"),
        vad_tier=mockingbird.VadTier.NEURAL if silero else mockingbird.VadTier.FAST,
        neural_vad_model_path=silero,
        anomaly=args.preset not in ("voice", "music"),
        sound_class=bool(yamnet) and args.preset not in ("voice", "anomaly"),
        sound_class_model_path=yamnet,
        encoder=mockingbird.EncoderConfig.opus() if args.preset in ("voice", "everything") else None,
        queues=mockingbird.QueueConfig(
            pcm_ring_frames=1 << 17 if args.file else 1 << 14,
            packet_queue_capacity=4096 if args.file else 256,
            pcm_tap=args.tap,
            pcm_tap_frames=1 << 16,
        ),
        performance=mockingbird.PerformanceConfig.mobile() if live else None,
    )
    if args.preset == "anomaly":
        cfg.anomaly.rms_z = cfg.anomaly.flux_z = 2.0
    if args.preset == "music":
        cfg.sound_class.threshold = 0.3
    if not silero:
        print("note: no silero_vad.onnx, using fast VAD", file=sys.stderr)
    if not yamnet:
        print("note: no yamnet_3s.onnx, sound classification off", file=sys.stderr)
    return cfg


def describe(cfg: mockingbird.Config, preset: str) -> str:
    """A one-line summary of what the engine will run."""
    on = []
    if cfg.vad.enabled:
        on.append(f"VAD:{cfg.vad.tier.name.lower()}")
    if cfg.anomaly.enabled:
        on.append("anomaly")
    if cfg.sound_class.enabled:
        on.append("YAMNet")
    if cfg.denoise:
        on.append("RNNoise")
    if cfg.queues.pcm_tap:
        on.append("tap")
    if cfg.performance.adaptive_degradation:
        on.append("adaptive")
    tag = f"preset={preset}, " if preset else ""
    backend = "host-pcm" if cfg.backend is mockingbird.Backend.HOST_PCM else "mic"
    codec = cfg.encoder.codec.name.lower()
    return f"{tag}{backend}, {cfg.sample_rate_hz} Hz, {codec}, [{' '.join(on)}]"
