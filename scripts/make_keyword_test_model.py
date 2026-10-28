"""Builds tests/fixtures/keyword_test.onnx, a keyword classifier in the
openWakeWord layout (16 embeddings of 96 values in, one score out) that
fires on speech in hello.wav. The pretrained openWakeWord keywords carry a
non-commercial licence, so the tests use this one instead. Its weights point
from the embeddings of the clip's silence towards those of its speech.
Needs numpy, onnx and onnxruntime.

    python3 scripts/make_keyword_test_model.py
"""

import wave
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
from onnx import TensorProto, helper, numpy_helper

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "packages" / "mockingbird" / "assets"


def embeddings(pcm):
    mel = ort.InferenceSession(str(ASSETS / "melspectrogram.onnx"))
    emb = ort.InferenceSession(str(ASSETS / "embedding_model.onnx"))
    raw, mels, out = np.zeros(1760, np.float32), np.ones((76, 32), np.float32), []
    for i in range(0, len(pcm) - 1279, 1280):
        raw = np.concatenate([raw, pcm[i : i + 1280].astype(np.float32)])[-1760:]
        m = np.squeeze(mel.run(None, {"input": raw[None]})[0]) / 10 + 2
        mels = np.vstack([mels, m])[-76:]
        out.append(emb.run(None, {"input_1": mels[None, :, :, None]})[0].reshape(96))
    return np.array(out)


def main():
    with wave.open(str(ROOT / "tests" / "fixtures" / "hello.wav")) as w:
        pcm = np.frombuffer(w.readframes(w.getnframes()), np.int16)
    e = embeddings(pcm)
    level = np.array([np.sqrt(np.mean(pcm[i : i + 1280].astype(np.float64) ** 2)) for i in range(0, len(e) * 1280, 1280)])
    loud = level > np.percentile(level, 70)
    direction = e[loud].mean(0) - e[~loud].mean(0)
    direction /= np.linalg.norm(direction)
    windows = np.array([e[k - 15 : k + 1].mean(0) for k in range(15, len(e))])
    proj = windows @ direction
    weights = np.tile(direction / 16, 16).astype(np.float32) * (8 / proj.std())
    bias = np.array([-(np.percentile(proj, 80) * 8 / proj.std())], np.float32)
    graph = helper.make_graph(
        [
            helper.make_node("Flatten", ["x"], ["flat"], axis=1),
            helper.make_node("Gemm", ["flat", "w", "b"], ["logit"], transB=1),
            helper.make_node("Sigmoid", ["logit"], ["score"]),
        ],
        "keyword_test",
        [helper.make_tensor_value_info("x", TensorProto.FLOAT, [1, 16, 96])],
        [helper.make_tensor_value_info("score", TensorProto.FLOAT, [1, 1])],
        [numpy_helper.from_array(weights[None, :], "w"), numpy_helper.from_array(bias, "b")],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)])
    model.ir_version = 8
    onnx.checker.check_model(model)
    onnx.save(model, str(ROOT / "tests" / "fixtures" / "keyword_test.onnx"))


if __name__ == "__main__":
    main()
