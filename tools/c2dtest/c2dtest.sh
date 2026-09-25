#!/usr/bin/env bash
# Phase-1 acceptance tests for the Canvas 2D shim. Software GL, no display.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
cmake -B build >/dev/null
cmake --build build --target c2dtest -j"$(nproc)" >/dev/null
mkdir -p build/preview
xvfb-run -a -s "-screen 0 1920x1080x24" ./build/tools/c2dtest "$@"
