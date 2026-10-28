#!/usr/bin/env bash
# Builds ONNX Runtime as one static library for Android, Linux or Windows,
# reduced to the operators in scripts/ort_ops.config, and packs it with its
# headers into an archive the platform packages download. ONNX Runtime's
# static build leaves a library per component plus its dependencies; they
# are merged into one, so a linker finds every symbol in one pass.
#   scripts/build_ort_static.sh <onnxruntime-1.30.0-checkout> <target> <out-dir>
# Targets: android-arm64-v8a, android-armeabi-v7a, android-x86_64,
# android-x86, linux-x64, linux-aarch64, windows-x64, windows-arm64.
# Android needs ANDROID_HOME and ANDROID_NDK_HOME; Windows runs under Git
# Bash with the MSVC tools on PATH. Needs Python with flatbuffers.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "$1" && pwd)"
TARGET="$2"
mkdir -p "$3"
OUT="$(cd "$3" && pwd)"
BUILD="$OUT/build-$TARGET"
NAME="onnxruntime-mockingbird-1.30.0-$TARGET"

args=(--config MinSizeRel --parallel --skip_tests --skip_submodule_sync
  --compile_no_warning_as_error --no_telemetry --disable_ml_ops --minimal_build extended
  --enable_reduced_operator_type_support
  --include_ops_by_config "$ROOT/scripts/ort_ops.config" --build_dir "$BUILD")
defines=(onnxruntime_BUILD_UNIT_TESTS=OFF CMAKE_POSITION_INDEPENDENT_CODE=ON
  FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER)
case "$TARGET" in
  android-*)
    args+=(--android --android_abi "${TARGET#android-}" --android_api 24
      --android_sdk_path "$ANDROID_HOME" --android_ndk_path "$ANDROID_NDK_HOME") ;;
  windows-arm64) args+=(--arm64) ;;
esac
case "$TARGET" in
  windows-*) ;;
  *)
    map="-ffile-prefix-map=$SRC=onnxruntime -ffile-prefix-map=$BUILD=build"
    defines+=("CMAKE_C_FLAGS=$map" "CMAKE_CXX_FLAGS=$map") ;;
esac

python3 "$SRC/tools/ci_build/build.py" "${args[@]}" --cmake_extra_defines "${defines[@]}"

PKG="$OUT/$NAME"
rm -rf "$PKG"
mkdir -p "$PKG/include" "$PKG/lib"
cp "$SRC"/include/onnxruntime/core/session/*.h "$PKG/include/"
cp "$SRC/LICENSE" "$SRC/ThirdPartyNotices.txt" "$PKG/"
skip='test|gtest|gmock|mocked|benchmark|onnx_proto|protobuf|protoc'
case "$TARGET" in
  windows-*)
    libs=$(find "$BUILD/MinSizeRel" -name '*.lib' | grep -Ev "$skip")
    # shellcheck disable=SC2086
    lib.exe -nologo -OUT:"$(cygpath -w "$PKG/lib/onnxruntime.lib")" $(for l in $libs; do cygpath -w "$l"; done)
    (cd "$OUT" && 7z a -tzip "$NAME.zip" "$NAME" > /dev/null)
    ARCHIVE="$OUT/$NAME.zip" ;;
  *)
    AR="${AR:-ar}"
    [ "${TARGET%%-*}" = android ] && AR="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$(ls "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt")/bin/llvm-ar"
    {
      echo "create $PKG/lib/libonnxruntime.a"
      find "$BUILD/MinSizeRel" -name '*.a' | grep -Ev "$skip" | sed 's/^/addlib /'
      echo save
      echo end
    } | "$AR" -M
    tar -C "$OUT" -czf "$OUT/$NAME.tgz" "$NAME"
    ARCHIVE="$OUT/$NAME.tgz" ;;
esac
ls -la "$ARCHIVE"
if command -v sha256sum > /dev/null; then sha256sum "$ARCHIVE"; else shasum -a 256 "$ARCHIVE"; fi
