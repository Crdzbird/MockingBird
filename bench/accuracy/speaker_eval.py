"""Speaker verification accuracy: how well the engine's voice prints tell
the 40 LibriSpeech test-clean speakers apart, by test length and noise.

Each speaker is enrolled with one clean utterance longer than 4 s. Their
other five utterances, cut to 1, 2 or 4 s and mixed with ESC-50 sounds that
contain no voice, are compared with all 40 enrollments: 200 target and
7,800 impostor trials per condition. The engine then runs whole utterances
through voice detection and the verifier, so the event path is checked too.

    python3 speaker_eval.py ~/.cache/mockingbird-bench
"""

import json
import sys
import time
from pathlib import Path

import numpy as np

import audio
import engine_vad
import vad_eval

MODEL = engine_vad.ROOT / "models" / "speaker_campplus.onnx"
mockingbird = engine_vad.mockingbird
SECONDS = [1, 2, 4]
SNRS = [None, 10, 0]
THRESHOLDS = [0.4, 0.5, 0.6]


def pcm(x):
    return (np.clip(x, -1, 1) * 32767).astype(np.int16)


def mix(x, noise, snr, rng):
    if snr is None:
        return x
    start = rng.integers(len(noise) - len(x))
    n = noise[start : start + len(x)]
    return x + n * np.sqrt(np.mean(x**2) / (np.mean(n**2) * 10 ** (snr / 10)))


def rates(tgt, non):
    tgt, non = np.array(tgt), np.array(non)
    eer = min(max(np.mean(tgt < t), np.mean(non >= t)) for t in np.unique(np.concatenate([tgt, non])))
    at = {t: {"false_reject": float(np.mean(tgt < t)), "false_accept": float(np.mean(non >= t))} for t in THRESHOLDS}
    return {"eer": float(eer), "at": at}


def events(e, x, target):
    e.set_target_speaker(target)
    e.start()
    for i in range(0, len(x) - audio.FRAME + 1, audio.FRAME):
        e.push_pcm(x[i : i + audio.FRAME], i * 1_000_000 // audio.RATE)
        while e.stats().pcm_ring_fill_frames > audio.FRAME * 256:
            time.sleep(0.001)
    e.stop()
    e.drain_packets()
    return [ev.confidence for ev in e.poll_events() if ev.type is mockingbird.EventType.SPEAKER]


def main(cache: Path):
    rng = np.random.default_rng(5)
    files = sorted((cache / "speakers").glob("*.wav"))
    speaker = lambda f: f.name.split("_")[0]
    noise = vad_eval.noise(cache, audio.RATE * 600, rng)
    cfg = mockingbird.Config(backend=mockingbird.Backend.HOST_PCM)
    cfg.vad.tier = mockingbird.VadTier.NEURAL
    cfg.vad.model_path = str(engine_vad.ROOT / "tests" / "fixtures" / "silero_vad.onnx")
    cfg.anomaly.enabled = False
    cfg.queues.pcm_ring_frames = 1 << 20
    results = {}
    with mockingbird.Engine(cfg) as e:
        e.set_speaker_model(MODEL)
        enroll, tests = {}, []
        for f in files:
            (tests.append(f) if speaker(f) in enroll else enroll.setdefault(speaker(f), e.speaker_embedding(pcm(audio.read(f)))))
        ids = sorted(enroll)
        bank = np.array([enroll[s] for s in ids])
        for snr in SNRS:
            for secs in SECONDS:
                tgt, non = [], []
                for f in tests:
                    x = audio.read(f)[int(0.3 * audio.RATE) :][: secs * audio.RATE]
                    v = np.array(e.speaker_embedding(pcm(mix(x, noise, snr, rng))))
                    for s, score in zip(ids, bank @ v):
                        (tgt if s == speaker(f) else non).append(float(score))
                label = f"{'clean' if snr is None else f'{snr} dB'}, {secs} s"
                results[label] = rates(tgt, non)
                print(label, results[label], flush=True)
        tgt, non = [], []
        for f in tests[::5]:
            x = pcm(mix(audio.read(f), noise, 10, rng))
            other = ids[(ids.index(speaker(f)) + 1) % len(ids)]
            tgt += events(e, x, enroll[speaker(f)])
            non += events(e, x, enroll[other])
        results["engine events, whole utterances, 10 dB"] = {"target": tgt, "impostor": non}
        print("engine events: target", np.round(tgt, 3), "impostor", np.round(non, 3), flush=True)
    (Path(__file__).parent / "results").mkdir(exist_ok=True)
    (Path(__file__).parent / "results" / "speaker.json").write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main(Path(sys.argv[1]).expanduser())
