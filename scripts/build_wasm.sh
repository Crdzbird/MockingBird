#!/usr/bin/env bash
# Builds the engine to WebAssembly twice with Emscripten, without and with
# SIMD, and copies mockingbird.js, mockingbird.wasm and mockingbird_simd.wasm
# into the mockingbird_web Flutter package. Both builds share one JavaScript
# glue file; the script stops if they ever differ.
#   scripts/build_wasm.sh            # release
#   scripts/build_wasm.sh Debug
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
OUT="$ROOT/packages/mockingbird_web/assets"

build() {
  emcmake cmake -S "$ROOT" -B "$1" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DMOCKINGBIRD_ENABLE_TESTS=OFF \
    -DOPUS_DISABLE_INTRINSICS=ON \
    -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= \
    -DMOCKINGBIRD_WASM_SIMD="$2" >/dev/null
  cmake --build "$1" --target mockingbird_wasm -j
}

build "$ROOT/build-wasm" OFF
build "$ROOT/build-wasm-simd" ON
cmp -s "$ROOT/build-wasm/mockingbird.js" "$ROOT/build-wasm-simd/mockingbird.js" || {
  echo "the SIMD build produced different JavaScript glue" >&2
  exit 1
}

mkdir -p "$OUT"
cp "$ROOT/build-wasm/mockingbird.js" "$ROOT/build-wasm/mockingbird.wasm" "$OUT/"
cp "$ROOT/build-wasm-simd/mockingbird.wasm" "$OUT/mockingbird_simd.wasm"
ls -la "$OUT"
