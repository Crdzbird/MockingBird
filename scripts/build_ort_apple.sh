#!/usr/bin/env bash
# Builds ONNX Runtime as a static xcframework for iOS, the iOS simulator and
# macOS, reduced to the operators in scripts/ort_ops.config, with ONNX
# Runtime's own build_apple_framework.py and the settings in
# scripts/ort_apple_build_settings.json. The SwiftPM packages of
# mockingbird_ios and mockingbird_macos link it as a binary target.
#   scripts/build_ort_apple.sh /path/to/onnxruntime-v1.30.0-checkout [out-dir]
# The checkout's tools need Python with flatbuffers installed. The patches in
# scripts/ort_patches are applied to the checkout first.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "$1" && pwd)"
OUT="${2:-$ROOT/build-ort-apple}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

for patch in "$ROOT"/scripts/ort_patches/*.patch; do
  git -C "$SRC" apply --reverse --check "$patch" 2> /dev/null || git -C "$SRC" apply "$patch"
done

python3 "$SRC/tools/ci_build/github/apple/build_apple_framework.py" \
  --build_dir "$OUT" --config MinSizeRel \
  --include_ops_by_config "$ROOT/scripts/ort_ops.config" \
  "$ROOT/scripts/ort_apple_build_settings.json"

FRAMEWORK="$OUT/framework_out/onnxruntime.xcframework"
(cd "$(dirname "$FRAMEWORK")" && rm -f onnxruntime.xcframework.zip && zip -qry onnxruntime.xcframework.zip onnxruntime.xcframework)
ls -la "$FRAMEWORK.zip"
shasum -a 256 "$FRAMEWORK.zip"
