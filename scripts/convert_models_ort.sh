#!/usr/bin/env bash
# Converts the bundled and the downloadable ONNX models to ONNX Runtime's ORT format with runtime
# optimizations, which the reduced ONNX Runtime the web build links can load,
# and writes the operator list that build is reduced to. Keyword models in
# the optional directory only add their operators to the list, so keyword
# models trained with openWakeWord run on the web; the committed list was
# made with hey_jarvis_v0.1.onnx from openWakeWord's v0.5.1 release.
# Needs Python with onnxruntime 1.30 or later.
#   scripts/convert_models_ort.sh [keyword-models-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS="$ROOT/packages/mockingbird/assets"
HOSTED="$ROOT/models"
MODELS="silero_vad melspectrogram embedding_model"
HOSTED_MODELS="yamnet_3s speaker_campplus"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for m in $MODELS; do cp "$ASSETS/$m.onnx" "$WORK/"; done
for m in $HOSTED_MODELS; do cp "$HOSTED/$m.onnx" "$WORK/"; done
cp "$ROOT/tests/fixtures/keyword_test.onnx" "$WORK/"
if [ -n "${1:-}" ]; then cp "$1"/*.onnx "$WORK/"; fi
python3 -m onnxruntime.tools.convert_onnx_models_to_ort "$WORK" --enable_type_reduction >/dev/null
for m in $MODELS; do cp "$WORK/$m.with_runtime_opt.ort" "$ASSETS/$m.ort"; done
for m in $HOSTED_MODELS; do cp "$WORK/$m.with_runtime_opt.ort" "$HOSTED/$m.ort"; done
cp "$WORK/keyword_test.with_runtime_opt.ort" "$ROOT/tests/fixtures/keyword_test.ort"
{
  echo "# Operators and types of the bundled and downloadable models and of"
  echo "# openWakeWord keyword models, with runtime optimizations, from"
  echo "# scripts/convert_models_ort.sh."
  echo "# ONNX Runtime builds that only need to run these models are reduced to"
  echo "# exactly this set."
  grep -v '^#' "$WORK/required_operators_and_types.with_runtime_opt.config"
} > "$ROOT/scripts/ort_ops.config"
ls -la "$ASSETS/"*.ort "$HOSTED/"*.ort
