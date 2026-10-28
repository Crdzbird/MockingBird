#!/usr/bin/env bash
# Converts the bundled ONNX models to ONNX Runtime's ORT format with runtime
# optimizations, which the reduced ONNX Runtime the web build links can load,
# and writes the operator list that build is reduced to.
# Needs Python with onnxruntime 1.30 or later.
#   scripts/convert_models_ort.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$ROOT/packages/mockingbird/assets/silero_vad.onnx" "$ROOT/packages/mockingbird/assets/yamnet_3s.onnx" "$WORK/"
python3 -m onnxruntime.tools.convert_onnx_models_to_ort "$WORK" --enable_type_reduction >/dev/null
cp "$WORK/silero_vad.with_runtime_opt.ort" "$ROOT/packages/mockingbird/assets/silero_vad.ort"
cp "$WORK/yamnet_3s.with_runtime_opt.ort" "$ROOT/packages/mockingbird/assets/yamnet_3s.ort"
{
  echo "# Operators and types of silero_vad.ort and yamnet_3s.ort with runtime"
  echo "# optimizations, from scripts/convert_models_ort.sh. ONNX Runtime builds that"
  echo "# only need to run these models are reduced to exactly this set."
  grep -v '^#' "$WORK/required_operators_and_types.with_runtime_opt.config"
} > "$ROOT/scripts/ort_ops.config"
ls -la "$ROOT/packages/mockingbird/assets/"*.ort
