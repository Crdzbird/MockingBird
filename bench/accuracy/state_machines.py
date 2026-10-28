"""The speech decision logic other Flutter packages put around Silero,
reimplemented from their published defaults. Each turns per-window
probabilities into speech intervals in seconds."""

WINDOW_S = 512 / 16000


def hysteresis(probs, positive=0.5, negative=0.35, redemption=24, min_speech=9, pre_pad=3):
    """The vad and vad_plus packages: speech starts on a window at or above
    `positive`, ends after `redemption` windows below `negative`, and a run
    with fewer than `min_speech` positive windows is dropped as a misfire."""
    out, speaking, start, positives, quiet = [], False, 0, 0, 0
    for i, p in enumerate(probs):
        if p >= positive:
            quiet = 0
            if not speaking:
                speaking, start, positives = True, max(0, i - pre_pad), 0
            positives += 1
        elif p < negative and speaking:
            quiet += 1
            if quiet >= redemption:
                if positives >= min_speech:
                    out.append((start * WINDOW_S, (i - redemption + 1) * WINDOW_S))
                speaking, quiet = False, 0
    if speaking and positives >= min_speech:
        out.append((start * WINDOW_S, len(probs) * WINDOW_S))
    return out


def sherpa(probs, threshold=0.5, min_speech_s=0.25, min_silence_s=0.5):
    """sherpa-onnx: speech starts once probabilities stay above `threshold`
    for `min_speech_s`, and ends once they stay below `threshold - 0.15` for
    `min_silence_s`."""
    need_speech = max(1, round(min_speech_s / WINDOW_S))
    need_silence = max(1, round(min_silence_s / WINDOW_S))
    out, speaking, run_start, above, below, start = [], False, 0, 0, 0, 0
    for i, p in enumerate(probs):
        if not speaking:
            if p > threshold:
                if above == 0:
                    run_start = i
                above += 1
                if above >= need_speech:
                    speaking, start, below = True, run_start, 0
            else:
                above = 0
            continue
        if p < threshold - 0.15:
            below += 1
            if below >= need_silence:
                out.append((start * WINDOW_S, (i - below + 1) * WINDOW_S))
                speaking, above = False, 0
        else:
            below = 0
    if speaking:
        out.append((start * WINDOW_S, len(probs) * WINDOW_S))
    return out
