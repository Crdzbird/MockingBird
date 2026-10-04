#!/usr/bin/env python3
"""A flag-driven tour of the Auris engine from Python.

    python3 example.py                                   default mic
    python3 example.py --preset everything --file ../../tests/fixtures/hello.wav
    python3 example.py --preset music --file ../../tests/fixtures/miaow_16k.wav
    python3 example.py --seconds 10 --sensitivity 0.8    live mic, eager
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import auris
from example_config import build_config, describe
from example_run import load_labels, run_file, run_mic


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--preset", default="", choices=["", "voice", "everything", "anomaly", "music"])
    ap.add_argument("--file", help="16-bit mono WAV to process via HOST_PCM")
    ap.add_argument("--sensitivity", type=float, default=0.6, help="0..1 detector dial")
    ap.add_argument("--seconds", type=int, default=15, help="mic capture duration")
    ap.add_argument("--tap", action="store_true", help="enable the continuous PCM tap")
    args = ap.parse_args()

    cfg = build_config(args)
    print(f"Auris {auris.version()} — {describe(cfg, args.preset)}")
    with auris.Engine(cfg) as engine:
        engine.start()
        if args.file:
            run_file(engine, args.file, load_labels())
        else:
            run_mic(engine, args.seconds, load_labels())


if __name__ == "__main__":
    main()
