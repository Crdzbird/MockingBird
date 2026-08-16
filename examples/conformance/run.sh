#!/usr/bin/env bash
# Build libauris + every peer, then run the conformance exchange across all
# three binding pairs:
#
#   Go     (listen) <-> Dart   (dial)
#   Go     (listen) <-> Python (dial)
#   Python (listen) <-> Dart   (dial)
#
# Each run is bidirectional, so every binding both produces audio for the others
# and processes theirs. Exits non-zero if any pair diverges.
#
#   ./run.sh              # base port 9999
#   PORT=9931 ./run.sh
#   PAIRS="go-python" ./run.sh    # run a subset
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$ROOT/examples/conformance"
PORT="${PORT:-9999}"
PAIRS="${PAIRS:-go-dart go-python python-dart}"
PYTHON="${PYTHON:-python3}"

echo "==> building libauris (shared)"
cmake -S "$ROOT" -B "$ROOT/build" -DAURIS_BUILD_SHARED=ON >/dev/null || exit 1
cmake --build "$ROOT/build" -j >/dev/null || exit 1

echo "==> building Go peer"
( cd "$HERE/go" && go build -o conformance . ) || exit 1

echo "==> resolving Dart peer deps"
( cd "$HERE/dart" && dart pub get >/dev/null ) || exit 1

failures=0
port=$PORT

run_pair() {
  local pair="$1" listener_cmd="$2" dialer_cmd="$3"
  echo
  echo "════════ $pair (127.0.0.1:$port) ════════"
  # shellcheck disable=SC2086
  eval "$listener_cmd" &
  local lpid=$!
  sleep 2
  # shellcheck disable=SC2086
  eval "$dialer_cmd"
  local drc=$?
  wait "$lpid"
  local lrc=$?
  if [ "$lrc" -eq 0 ] && [ "$drc" -eq 0 ]; then
    echo "-------- $pair: PASS"
  else
    echo "-------- $pair: FAIL (listener rc=$lrc, dialer rc=$drc)"
    failures=$((failures + 1))
  fi
  port=$((port + 1))
}

for pair in $PAIRS; do
  case "$pair" in
    go-dart)
      run_pair "Go <-> Dart" \
        "\"$HERE/go/conformance\" -addr 127.0.0.1:$port -peer dart" \
        "( cd \"$HERE/dart\" && dart run conformance.dart 127.0.0.1:$port --peer=go )"
      ;;
    go-python)
      run_pair "Go <-> Python" \
        "\"$HERE/go/conformance\" -addr 127.0.0.1:$port -peer python" \
        "$PYTHON \"$HERE/python/conformance.py\" dial 127.0.0.1:$port go"
      ;;
    python-dart)
      run_pair "Python <-> Dart" \
        "$PYTHON \"$HERE/python/conformance.py\" listen $port dart" \
        "( cd \"$HERE/dart\" && dart run conformance.dart 127.0.0.1:$port --peer=python )"
      ;;
    *)
      echo "unknown pair: $pair" >&2
      failures=$((failures + 1))
      ;;
  esac
done

echo
if [ "$failures" -eq 0 ]; then
  echo "✅ conformance passed — every binding pair is byte-identical"
  exit 0
fi
echo "❌ conformance failed in $failures pair(s)"
exit 1
