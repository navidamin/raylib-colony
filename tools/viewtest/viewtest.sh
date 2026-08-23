#!/usr/bin/env bash
# Headless screenshots of the whole view ladder.
#   tools/viewtest/viewtest.sh            -> build/viewtest/vt_*.png
#   tools/viewtest/viewtest.sh myprefix
set -euo pipefail
cd "$(dirname "$0")/../.."
PREFIX="${1:-build/viewtest/vt}"
mkdir -p "$(dirname "$PREFIX")"
cmake --build build --target colony_viewtest -j"$(nproc)"
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    xvfb-run -a ./build/src/colony_viewtest --shots "$PREFIX"
echo "wrote ${PREFIX}_{orbital,planet,colony,sect}.png"
