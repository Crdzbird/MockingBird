#!/usr/bin/env bash
# Builds the engine to WebAssembly with Emscripten and copies auris.js and
# auris.wasm into the auris_web Flutter package.
#   scripts/build_wasm.sh            # release
#   scripts/build_wasm.sh Debug
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
OUT="$ROOT/packages/auris_web/assets"

emcmake cmake -S "$ROOT" -B "$ROOT/build-wasm" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DAURIS_ENABLE_TESTS=OFF \
  -DOPUS_DISABLE_INTRINSICS=ON >/dev/null
cmake --build "$ROOT/build-wasm" --target auris_wasm -j

mkdir -p "$OUT"
cp "$ROOT/build-wasm/auris.js" "$ROOT/build-wasm/auris.wasm" "$OUT/"
ls -la "$OUT"
