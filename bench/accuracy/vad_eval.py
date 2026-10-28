"""Voice activity detection accuracy at several signal-to-noise ratios.

A timeline of LibriSpeech utterances with 1 to 3 s gaps is labelled frame by
frame from the clean signal, then mixed with ESC-50 sounds that contain no
voice. Every system turns the mixture into speech intervals, scored on the
20 ms grid as precision, recall and F1 of speech frames, plus how often noise
alone was called speech.

    python3 vad_eval.py ~/.cache/mockingbird-bench
"""

import csv
import json
import sys
from pathlib import Path

import numpy as np

import audio
import engine_vad
import silero
import state_machines

PUB = Path.home() / ".pub-cache" / "hosted" / "pub.dev"
SILERO_V5 = PUB / "vad-0.0.8" / "companion-package" / "silero_vad_v5.onnx"
SILERO_V6 = engine_vad.ROOT / "tests" / "fixtures" / "silero_vad.onnx"
VOCAL = {"crying_baby", "laughing", "coughing", "sneezing", "breathing", "snoring", "drinking_sipping"}
SNRS = [None, 20, 10, 5, 0]


def timeline(cache: Path, rng):
    speech, labels = [], []
    for f in sorted((cache / "speech").glob("*.wav")):
        gap = np.zeros(int(rng.uniform(1, 3) * audio.RATE), np.float32)
        utt = audio.read(f)
        speech += [gap, utt]
        db = audio.rms_db(utt)
        labels += [np.zeros(audio.frames(gap), bool), db > max(db.max() - 35, -55)]
    tail = np.zeros(2 * audio.RATE, np.float32)
    signal = np.concatenate(speech + [tail])
    truth = np.concatenate(labels + [np.zeros(audio.frames(tail), bool)])
    return signal, smooth(truth, gap_frames=10, min_frames=3)


def smooth(mask, gap_frames, min_frames):
    m = mask.copy()
    idx = np.flatnonzero(np.diff(np.concatenate([[0], m.astype(int), [0]])))
    runs = list(zip(idx[::2], idx[1::2]))
    for (a0, a1), (b0, _) in zip(runs, runs[1:]):
        if b0 - a1 < gap_frames:
            m[a1:b0] = True
    idx = np.flatnonzero(np.diff(np.concatenate([[0], m.astype(int), [0]])))
    for a, b in zip(idx[::2], idx[1::2]):
        if b - a < min_frames:
            m[a:b] = False
    return m


def noise(cache: Path, length: int, rng):
    with open(cache / "esc50" / "meta.csv") as f:
        files = [r["filename"] for r in csv.DictReader(f) if r["category"] not in VOCAL]
    parts, n = [], 0
    while n < length:
        clip = audio.read(cache / "esc50" / files[rng.integers(len(files))])
        parts.append(clip)
        n += len(clip)
    return np.concatenate(parts)[:length]


def to_frames(intervals, n):
    m = np.zeros(n, bool)
    for a, b in intervals:
        m[int(a * audio.RATE / audio.FRAME) : int(b * audio.RATE / audio.FRAME)] = True
    return m


def score(pred, truth):
    tp = np.sum(pred & truth)
    precision = tp / max(1, np.sum(pred))
    recall = tp / max(1, np.sum(truth))
    f1 = 2 * precision * recall / max(1e-9, precision + recall)
    false_alarm = np.sum(pred & ~truth) / max(1, np.sum(~truth))
    return {"precision": precision, "recall": recall, "f1": f1, "false_alarm": false_alarm}


SYSTEMS = {
    "Mockingbird fast (libfvad)": lambda x: engine_vad.intervals(x, neural=False),
    "Mockingbird neural (Silero v6.2)": lambda x: engine_vad.intervals(x, neural=True),
    "Mockingbird neural, 768 ms hangover": lambda x: engine_vad.intervals(x, neural=True, hangover_ms=768),
    "vad (Silero v5)": lambda x: state_machines.hysteresis(silero.probabilities(str(SILERO_V5), x)),
    "vad_plus (Silero v6)": lambda x: state_machines.hysteresis(silero.probabilities(str(SILERO_V6), x)),
    "sherpa_onnx (Silero v5)": lambda x: state_machines.sherpa(silero.probabilities(str(SILERO_V5), x)),
}


def main(cache: Path):
    rng = np.random.default_rng(1234)
    clean, truth = timeline(cache, rng)
    n = len(truth)
    hum = noise(cache, len(clean), rng)
    speech_power = np.mean(clean[: n * audio.FRAME].reshape(n, -1)[truth] ** 2)
    results = {}
    for snr in SNRS:
        mix = clean
        if snr is not None:
            gain = np.sqrt(speech_power / (np.mean(hum**2) * 10 ** (snr / 10)))
            mix = np.clip(clean + gain * hum, -1, 1)
        label = "clean" if snr is None else f"{snr} dB"
        for name, run in SYSTEMS.items():
            results.setdefault(name, {})[label] = score(to_frames(run(mix), n), truth)
            print(f"{label:6} {name:34} {results[name][label]}", flush=True)
    minutes = len(clean) / audio.RATE / 60
    out = {"minutes": minutes, "speech_share": float(truth.mean()), "results": results}
    (Path(__file__).parent / "results").mkdir(exist_ok=True)
    (Path(__file__).parent / "results" / "vad.json").write_text(json.dumps(out, indent=2, default=float))


if __name__ == "__main__":
    main(Path(sys.argv[1]).expanduser())
