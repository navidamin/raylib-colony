#!/usr/bin/env bash
#
# Render a game view to a PNG without a display (xvfb + software OpenGL).
#
#   tools/preview/preview.sh --view orbital
#   tools/preview/preview.sh --all
#
# Other flags are forwarded to colony_preview (see --help).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/build/preview}"
BINARY="$BUILD_DIR/src/colony_preview"

cd "$REPO_ROOT"
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

Build()
{
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" >/dev/null
    fi
    cmake --build "$BUILD_DIR" --target colony_preview -j"$(nproc)" >/dev/null
}

Render() { xvfb-run -a -s "-screen 0 1920x1080x24" "$BINARY" "$@"; }

mkdir -p "$OUT_DIR"
Build

if [ "${1:-}" = "--all" ]; then
    for v in orbital planet; do
        Render --view "$v" --out "$OUT_DIR/view-$v.png"
    done
    ls -1 "$OUT_DIR"
    exit 0
fi

if [[ " $* " != *" --out "* ]]; then
    Render "$@" --out "$OUT_DIR/preview.png"
else
    Render "$@"
fi
