"""Runs a Silero VAD model the way its own Python wrapper does: 512-sample
windows at 16 kHz with the previous 64 samples prepended, carrying the
recurrent state, and returns one speech probability per window."""

import numpy as np
import onnxruntime as ort

WINDOW = 512
CONTEXT = 64


def probabilities(model: str, signal: np.ndarray) -> np.ndarray:
    o = ort.SessionOptions()
    o.intra_op_num_threads = 1
    s = ort.InferenceSession(model, o, providers=["CPUExecutionProvider"])
    names = [i.name for i in s.get_inputs()]
    state = np.zeros((2, 1, 128), np.float32)
    context = np.zeros(CONTEXT, np.float32)
    sr = np.array(16000, np.int64)
    out = []
    for start in range(0, len(signal) - WINDOW + 1, WINDOW):
        chunk = signal[start : start + WINDOW]
        x = np.concatenate([context, chunk])[None, :].astype(np.float32)
        feeds = {names[0]: x, "state": state, "sr": sr}
        prob, state = s.run(None, feeds)
        out.append(float(prob.reshape(-1)[0]))
        context = chunk[-CONTEXT:]
    return np.array(out)
