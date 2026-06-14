#!/usr/bin/env bash
# Build libauris + both peers, then run the Go<->Dart conformance exchange.
# Exits non-zero if either peer reports a divergence.
#
#   ./run.sh            # uses port 9999
#   PORT=9931 ./run.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PORT="${PORT:-9999}"

echo "==> building libauris (shared)"
cmake -S "$ROOT" -B "$ROOT/build" -DAURIS_BUILD_SHARED=ON >/dev/null || exit 1
cmake --build "$ROOT/build" -j >/dev/null || exit 1

echo "==> building Go peer"
( cd "$ROOT/examples/conformance/go" && go build -o conformance . ) || exit 1

echo "==> resolving Dart peer deps"
( cd "$ROOT/examples/conformance/dart" && dart pub get >/dev/null ) || exit 1

echo "==> running conformance on 127.0.0.1:$PORT"
"$ROOT/examples/conformance/go/conformance" -addr "127.0.0.1:$PORT" &
GOPID=$!
sleep 1.5

( cd "$ROOT/examples/conformance/dart" && dart run conformance.dart "127.0.0.1:$PORT" )
DART_RC=$?

wait "$GOPID"
GO_RC=$?

echo "==> go peer rc=$GO_RC, dart peer rc=$DART_RC"
if [ "$GO_RC" -eq 0 ] && [ "$DART_RC" -eq 0 ]; then
  echo "✅ conformance passed"
  exit 0
fi
echo "❌ conformance failed"
exit 1
