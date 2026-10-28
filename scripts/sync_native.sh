#!/usr/bin/env bash
# Copies the engine sources into the platform packages so each one is
# self-contained on pub.dev. The repository root stays the only place to edit;
# run this after any change under include/, src/ or third_party/ and commit
# the result. CI fails if the copies drift.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PARTS=(miniaudio libfvad pffft opus rnnoise)

sync_into() {
  local dest="$1"
  rm -rf "$dest/include" "$dest/src" "$dest/third_party"
  mkdir -p "$dest/include" "$dest/third_party"
  cp -R "$ROOT/include/mockingbird" "$dest/include/"
  cp -R "$ROOT/src" "$dest/src"
  for part in "${PARTS[@]}"; do
    cp -R "$ROOT/third_party/$part" "$dest/third_party/"
  done
  find "$dest" -name '.DS_Store' -delete
}

sync_into "$ROOT/packages/mockingbird_android/android/src/main/native"
sync_into "$ROOT/packages/mockingbird_ios/ios/mockingbird_ios/Sources/mockingbird_native"
sync_into "$ROOT/packages/mockingbird_macos/macos/mockingbird_macos/Sources/mockingbird_native"
for desktop in linux windows; do
  dest="$ROOT/packages/mockingbird_$desktop/$desktop/native"
  sync_into "$dest"
  cp "$ROOT/CMakeLists.txt" "$dest/CMakeLists.txt"
done
echo "native sources synced"
