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
  cp -R "$ROOT/include/auris" "$dest/include/"
  cp -R "$ROOT/src" "$dest/src"
  for part in "${PARTS[@]}"; do
    cp -R "$ROOT/third_party/$part" "$dest/third_party/"
  done
  find "$dest" -name '.DS_Store' -delete
}

sync_into "$ROOT/packages/auris_android/android/src/main/native"
sync_into "$ROOT/packages/auris_ios/ios/auris_ios/Sources/auris_native"
sync_into "$ROOT/packages/auris_macos/macos/auris_macos/Sources/auris_native"
echo "native sources synced"
