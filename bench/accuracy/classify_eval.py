"""Sound classification accuracy on ESC-50: Mockingbird's YAMNet with int8
weights, the float model it was made from, and CED tiny as sherpa_onnx runs
it. Both taggers know the 527 AudioSet classes, not ESC-50's, so each clip is
scored zero-shot: a category's score is the highest score of the AudioSet
labels mapped to it, and the clip counts as correct when its own category ranks
first, or among the first three, of the 47 mapped categories.

    python3 classify_eval.py ~/.cache/mockingbird-bench path/to/yamnet_float.onnx
"""

import csv
import json
import sys
from pathlib import Path

import numpy as np
import onnxruntime as ort
import sherpa_onnx

import audio
from esc50_audioset import MAPPING

ROOT = Path(__file__).resolve().parents[2]
CED = ROOT / "bench" / "flutter_compare" / "models" / "sherpa-onnx-ced-tiny-audio-tagging-2024-04-19"


def yamnet_labels():
    lines = (ROOT / "tests" / "fixtures" / "yamnet_class_map.txt").read_text().splitlines()
    return [l.split(" ", 1)[1] for l in lines if l.strip()]


def ced_labels():
    with open(CED / "class_labels_indices.csv") as f:
        return [r["display_name"] for r in csv.DictReader(f)]


def check(labels, name):
    missing = sorted({n for names in MAPPING.values() for n in names} - set(labels))
    if missing:
        raise SystemExit(f"{name} lacks {missing}")


def yamnet(model):
    o = ort.SessionOptions()
    o.intra_op_num_threads = 1
    s = ort.InferenceSession(str(model), o, providers=["CPUExecutionProvider"])

    def run(clip):
        clip = np.pad(clip, (0, max(0, 5 * audio.RATE - len(clip))))
        windows = [clip[k * audio.RATE : k * audio.RATE + 3 * audio.RATE] for k in range(3)]
        return np.max([s.run(["scores"], {"new_input": w[None, :]})[0].max(0) for w in windows], 0)

    return run


def ced():
    cfg = sherpa_onnx.AudioTaggingConfig(
        model=sherpa_onnx.AudioTaggingModelConfig(ced=str(CED / "model.onnx"), num_threads=1, provider="cpu"),
        labels=str(CED / "class_labels_indices.csv"),
        top_k=527,
    )
    tagger = sherpa_onnx.AudioTagging(cfg)

    def run(clip):
        stream = tagger.create_stream()
        stream.accept_waveform(sample_rate=audio.RATE, waveform=clip)
        scores = np.zeros(527, dtype=np.float32)
        for e in tagger.compute(stream):
            scores[e.index] = e.prob
        return scores

    return run


def evaluate(cache, classify, labels):
    with open(cache / "esc50" / "meta.csv") as f:
        rows = [r for r in csv.DictReader(f) if r["category"] in MAPPING]
    categories = sorted(MAPPING)
    columns = [[labels.index(n) for n in MAPPING[c]] for c in categories]
    top1 = top3 = 0
    per_class = {}
    for r in rows:
        scores = classify(audio.read(cache / "esc50" / r["filename"]))
        ranked = [categories[i] for i in np.argsort([-scores[c].max() for c in columns])]
        hit1, hit3 = ranked[0] == r["category"], r["category"] in ranked[:3]
        top1 += hit1
        top3 += hit3
        per_class.setdefault(r["category"], []).append(hit1)
    worst = sorted(per_class, key=lambda c: np.mean(per_class[c]))[:5]
    return {
        "clips": len(rows),
        "top1": top1 / len(rows),
        "top3": top3 / len(rows),
        "hardest": {c: float(np.mean(per_class[c])) for c in worst},
    }


def main(cache, float_model):
    y, c = yamnet_labels(), ced_labels()
    check(y, "YAMNet")
    check(c, "CED")
    systems = {
        "Mockingbird YAMNet (int8 weights)": (yamnet(ROOT / "models" / "yamnet_3s.onnx"), y),
        "YAMNet (float weights)": (yamnet(float_model), y),
        "sherpa_onnx CED tiny": (ced(), c),
    }
    results = {}
    for name, (classify, labels) in systems.items():
        results[name] = evaluate(cache, classify, labels)
        print(name, results[name], flush=True)
    (Path(__file__).parent / "results").mkdir(exist_ok=True)
    (Path(__file__).parent / "results" / "classify.json").write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main(Path(sys.argv[1]).expanduser(), Path(sys.argv[2]).expanduser())
