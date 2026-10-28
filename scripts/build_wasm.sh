#!/usr/bin/env bash
# Builds the engine to WebAssembly twice with Emscripten, without and with
# SIMD, and copies each build's glue and module into the mockingbird_web
# Flutter package: mockingbird.js and mockingbird.wasm, mockingbird_simd.js
# and mockingbird_simd.wasm. With ORT_WASM set to the output of
# scripts/build_ort_wasm.sh, both builds link ONNX Runtime and run Silero VAD
# and YAMNet in the browser.
#   scripts/build_wasm.sh            # release
#   scripts/build_wasm.sh Debug
#   ORT_WASM=build-ort-wasm scripts/build_wasm.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
OUT="$ROOT/packages/mockingbird_web/assets"

ORT_WASM="${ORT_WASM:-}"

neural() {
  [ -n "$ORT_WASM" ] || return 0
  local dir
  dir="$(cd "$ORT_WASM" && pwd)"
  echo "-DMOCKINGBIRD_ENABLE_NEURAL_VAD=ON" \
       "-DONNXRUNTIME_INCLUDE_DIR=$dir/include" \
       "-DONNXRUNTIME_LIBRARY=$dir/$1/MinSizeRel/libonnxruntime_webassembly.a"
}

build() {
  # shellcheck disable=SC2046
  emcmake cmake -S "$ROOT" -B "$1" $(neural "$3") \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DMOCKINGBIRD_ENABLE_TESTS=OFF \
    -DOPUS_DISABLE_INTRINSICS=ON \
    -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= \
    -DMOCKINGBIRD_WASM_SIMD="$2" >/dev/null
  cmake --build "$1" --target mockingbird_wasm -j
}

build "$ROOT/build-wasm" OFF scalar
build "$ROOT/build-wasm-simd" ON simd

mkdir -p "$OUT"
cp "$ROOT/build-wasm/mockingbird.js" "$ROOT/build-wasm/mockingbird.wasm" "$OUT/"
cp "$ROOT/build-wasm-simd/mockingbird.js" "$OUT/mockingbird_simd.js"
cp "$ROOT/build-wasm-simd/mockingbird.wasm" "$OUT/mockingbird_simd.wasm"
ls -la "$OUT"
