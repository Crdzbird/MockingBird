#!/usr/bin/env bash
# Builds each benchmark app in release mode, runs it, and records the size of
# the resulting .app bundle next to the measurements.
#   ./run.sh                 # every app
#   ./run.sh sherpa_bench    # one app
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
APPS="${*:-mockingbird_bench vad_plus_bench vad_bench sherpa_bench nnnoiseless_bench}"
mkdir -p "$HERE/results"

for app in $APPS; do
  echo "==> $app"
  (
    cd "$HERE/$app"
    flutter build macos --release --dart-define=MOCKINGBIRD_ROOT="$ROOT" >/dev/null
    bundle="build/macos/Build/Products/Release/$app.app"
    size_kb=$(du -sk "$bundle" | cut -f1)
    echo "{\"app\":\"$app\",\"bundle_kb\":$size_kb}" > "$HERE/results/$app.size.json"
    "$bundle/Contents/MacOS/$app" 2>&1 | grep -E '^BENCH|Error|error|Exception' || true
  )
done
