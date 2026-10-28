#!/usr/bin/env bash
# Builds ONNX Runtime as two static WebAssembly libraries, with and without
# SIMD, reduced to the operators in scripts/ort_ops.config, for
# scripts/build_wasm.sh to link. Uses the Emscripten on PATH rather than the
# emsdk ONNX Runtime would download, so both sides share one toolchain.
# Source and build paths are mapped to "onnxruntime" and "build", so no path
# from the machine that built it ends up in the module.
#   scripts/build_ort_wasm.sh /path/to/onnxruntime-v1.30.0-checkout [out-dir]
# The checkout's tools need Python with flatbuffers installed.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "$1" && pwd)"
OUT="${2:-$ROOT/build-ort-wasm}"
EMROOT="$(dirname "$(dirname "$(python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$(command -v emcc)")")")/libexec"

EMSDK_DIR="$SRC/cmake/external/emsdk"
mkdir -p "$EMSDK_DIR/upstream"
ln -sfn "$EMROOT" "$EMSDK_DIR/upstream/emscripten"
printf '#!/bin/sh\nexit 0\n' > "$EMSDK_DIR/emsdk"
printf '#!/bin/sh\n' > "$EMSDK_DIR/emsdk_env.sh"
chmod +x "$EMSDK_DIR/emsdk"

build() {
  local map="-ffile-prefix-map=$SRC=onnxruntime -ffile-prefix-map=$1=build"
  EMSDK="$EMSDK_DIR" EMSDK_NODE="$(command -v node)" EMSDK_PYTHON="$(command -v python3)" \
  python3 "$SRC/tools/ci_build/build.py" --config MinSizeRel --build_wasm_static_lib \
    --parallel --skip_tests --skip_submodule_sync --compile_no_warning_as_error --no_telemetry \
    --disable_ml_ops --minimal_build extended --enable_reduced_operator_type_support \
    --include_ops_by_config "$ROOT/scripts/ort_ops.config" --targets bundling_target \
    --build_dir "$1" "${@:2}" \
    --cmake_extra_defines FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER CMAKE_IGNORE_PREFIX_PATH=/opt/homebrew \
      "CMAKE_C_FLAGS=$map" "CMAKE_CXX_FLAGS=$map"
}

build "$OUT/simd" --enable_wasm_simd
build "$OUT/scalar"
mkdir -p "$OUT/include"
cp "$SRC"/include/onnxruntime/core/session/*.h "$OUT/include/"
ls -la "$OUT"/*/MinSizeRel/libonnxruntime_webassembly.a
