"""Unpacks LibriSpeech test-clean and ESC-50 from the cache and writes 16 kHz
mono WAV copies the evaluations read. Needs ffmpeg on PATH.

    python3 prepare.py ~/.cache/mockingbird-bench
"""

import csv
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path


def to_wav(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        return
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-i", str(src), "-ac", "1", "-ar", "16000", "-sample_fmt", "s16", str(dst)],
        check=True,
    )


def librispeech(cache: Path, utterances: int) -> None:
    raw = cache / "LibriSpeech"
    if not raw.exists():
        with tarfile.open(cache / "test-clean.tar.gz") as t:
            t.extractall(cache, filter="data")
    flacs = sorted((raw / "test-clean").rglob("*.flac"))
    speakers: dict[str, list[Path]] = {}
    for f in flacs:
        speakers.setdefault(f.parts[-3], []).append(f)
    picked = []
    while len(picked) < utterances and any(speakers.values()):
        for files in speakers.values():
            if files and len(picked) < utterances:
                picked.append(files.pop(0))
    for f in picked:
        to_wav(f, cache / "speech" / f"{f.stem}.wav")


def esc50(cache: Path) -> None:
    root = cache / "ESC-50-master"
    if not root.exists():
        with zipfile.ZipFile(cache / "esc50.zip") as z:
            z.extractall(cache)
    with open(root / "meta" / "esc50.csv") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        to_wav(root / "audio" / r["filename"], cache / "esc50" / r["filename"])
    with open(cache / "esc50" / "meta.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["filename", "category"])
        w.writeheader()
        for r in rows:
            w.writerow({"filename": r["filename"], "category": r["category"]})


if __name__ == "__main__":
    cache = Path(sys.argv[1]).expanduser()
    librispeech(cache, int(sys.argv[2]) if len(sys.argv) > 2 else 60)
    esc50(cache)
    print("prepared", cache)
