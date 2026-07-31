#!/usr/bin/env bash
# dev-test.sh — run the googletest suite (chatterino-test, registered with ctest
# via gtest_discover_tests) headlessly under xvfb, since the VPS has no display.
set -euo pipefail

cd "$(dirname "$0")/.."

if [ ! -f build/CMakeCache.txt ]; then
    echo "error: no configured build dir. Run ./scripts/dev-build.sh first." >&2
    exit 1
fi

exec xvfb-run -a ctest --test-dir build --output-on-failure "$@"
