#!/usr/bin/env bash
# Builds the engine to WebAssembly with Emscripten and copies mockingbird.js and
# mockingbird.wasm into the mockingbird_web Flutter package.
#   scripts/build_wasm.sh            # release
#   scripts/build_wasm.sh Debug
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
OUT="$ROOT/packages/mockingbird_web/assets"

emcmake cmake -S "$ROOT" -B "$ROOT/build-wasm" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DMOCKINGBIRD_ENABLE_TESTS=OFF \
  -DOPUS_DISABLE_INTRINSICS=ON >/dev/null
cmake --build "$ROOT/build-wasm" --target mockingbird_wasm -j

mkdir -p "$OUT"
cp "$ROOT/build-wasm/mockingbird.js" "$ROOT/build-wasm/mockingbird.wasm" "$OUT/"
ls -la "$OUT"
