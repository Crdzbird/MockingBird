#!/usr/bin/env bash
# Builds ONNX Runtime as a static xcframework for iOS, the iOS simulator and
# macOS, reduced to the operators in scripts/ort_ops.config, with ONNX
# Runtime's own build_apple_framework.py and the settings in
# scripts/ort_apple_build_settings.json. Its static frameworks are repacked as
# static libraries with headers, so Xcode links them and does not try to
# embed them in the app. The SwiftPM packages of mockingbird_ios and
# mockingbird_macos link the result as a binary target.
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

LIBS="$OUT/libraries"
rm -rf "$LIBS" "$OUT/onnxruntime.xcframework" "$OUT/onnxruntime.xcframework.zip"
slices=()
for framework in "$OUT"/framework_out/onnxruntime.xcframework/*/onnxruntime.framework; do
  slice="$LIBS/$(basename "$(dirname "$framework")")"
  mkdir -p "$slice"
  cp "$framework/onnxruntime" "$slice/libonnxruntime.a"
  cp -R "$framework/Headers" "$slice/include"
  slices+=(-library "$slice/libonnxruntime.a" -headers "$slice/include")
done
xcodebuild -create-xcframework "${slices[@]}" -output "$OUT/onnxruntime.xcframework"
(cd "$OUT" && zip -qry onnxruntime.xcframework.zip onnxruntime.xcframework)
ls -la "$OUT/onnxruntime.xcframework.zip"
shasum -a 256 "$OUT/onnxruntime.xcframework.zip"
