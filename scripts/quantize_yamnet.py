"""Stores YAMNet's convolution and dense weights as int8.

Each weight tensor becomes int8 values with one float scale per output
channel, followed by a DequantizeLinear node. ONNX Runtime folds those nodes
when the session loads, so inference still runs in float at the same speed
while the file is about a third of the size. The mel filterbank layer stays
in float. Needs the onnx and numpy packages.

    python3 scripts/quantize_yamnet.py float.onnx yamnet_3s.onnx
"""

import sys

import numpy as np
import onnx
from onnx import helper, numpy_helper, version_converter


def channel_axis(node):
    if node.op_type == "Conv":
        return 0
    trans_b = any(a.name == "transB" and a.i == 1 for a in node.attribute)
    return 0 if trans_b else 1


def quantize(src, dst):
    model = version_converter.convert_version(onnx.load(src), 13)
    inits = {t.name: t for t in model.graph.initializer}
    done, new_inits, new_nodes = set(), [], []
    for node in model.graph.node:
        if node.op_type not in ("Conv", "Gemm") or "tf_op_layer_MatMul" in node.name:
            continue
        name = node.input[1]
        if name not in inits or name in done:
            continue
        w = numpy_helper.to_array(inits[name]).astype(np.float32)
        axis = channel_axis(node)
        reduce = tuple(i for i in range(w.ndim) if i != axis)
        scale = np.maximum(np.abs(w).max(axis=reduce), 1e-12) / 127.0
        shape = [1] * w.ndim
        shape[axis] = -1
        q = np.clip(np.round(w / scale.reshape(shape)), -127, 127).astype(np.int8)
        new_inits += [
            numpy_helper.from_array(q, name + "_q"),
            numpy_helper.from_array(scale.astype(np.float32), name + "_s"),
            numpy_helper.from_array(np.zeros(scale.shape, np.int8), name + "_z"),
        ]
        new_nodes.append(helper.make_node(
            "DequantizeLinear", [name + "_q", name + "_s", name + "_z"], [name],
            axis=axis, name=name + "_dq"))
        done.add(name)
    keep = [t for t in model.graph.initializer if t.name not in done]
    del model.graph.initializer[:]
    model.graph.initializer.extend(keep + new_inits)
    nodes = list(model.graph.node)
    del model.graph.node[:]
    model.graph.node.extend(new_nodes + nodes)
    onnx.checker.check_model(model)
    onnx.save(model, dst)
    return len(done)


if __name__ == "__main__":
    print(f"{quantize(sys.argv[1], sys.argv[2])} weight tensors stored as int8")
