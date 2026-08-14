#!/usr/bin/env bash
#
# Render a unit module panel to a PNG without a display.
#
# Wraps the colony_preview binary in a virtual X server with software OpenGL,
# so it works over SSH, in CI, and inside containers. Builds the tool if it is
# missing or out of date, then writes the PNG.
#
#   tools/preview/preview.sh --module prospecting --tab lab --tier 3
#   tools/preview/preview.sh --all
#
# All other flags are forwarded to colony_preview (see --help).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/build/preview}"
BINARY="$BUILD_DIR/src/colony_preview"

cd "$REPO_ROOT"

# Software rendering: no GPU required.
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

Build()
{
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" >/dev/null
    fi
    # Only the preview target -- raylib is already built, so this is incremental.
    cmake --build "$BUILD_DIR" --target colony_preview -j"$(nproc)" >/dev/null
}

# Runs colony_preview under a throwaway X server.
Render()
{
    xvfb-run -a -s "-screen 0 1920x1080x24" "$BINARY" "$@"
}

mkdir -p "$OUT_DIR"

if [ "${1:-}" = "--all" ]; then
    Build
    echo "Rendering preview set into $OUT_DIR"

    for tab in sweep samples lab; do
        Render --module prospecting --tab "$tab" --state analyzed --tier 3 \
               --out "$OUT_DIR/prospecting-$tab-t3.png"
    done

    for state in empty swept sampled analyzed; do
        Render --module prospecting --tab sweep --state "$state" --tier 3 \
               --out "$OUT_DIR/prospecting-sweep-$state.png"
    done

    for module in excavation beneficiation operations directives overview; do
        Render --module "$module" --tier 3 \
               --out "$OUT_DIR/$module-t3.png"
    done

    # One panel per module for the units that still use the generic panel, so a
    # chrome or icon regression shows up on any of the 20 stub modules.
    RenderUnit()
    {
        unit="$1"
        shift
        for module in "$@"; do
            Render --unit "$unit" --module "$module" --tier 2 \
                   --out "$OUT_DIR/${unit,,}-$module-t2.png"
        done
    }

    RenderUnit Farming       irrigation greenhouse hydroponics harvest storage
    RenderUnit Energy        solar battery nuclear grid emergency
    RenderUnit Manufacture   fabrication assembly quality logistics automation
    RenderUnit Research      laboratory analysis simulation archive publication
    RenderUnit Construction  siteprep foundation structures fitout maintenance
    RenderUnit Transport     fleet routing depot servicing dispatch
    RenderUnit Core          lifesupport roster command monitoring safety

    Render --module sprites --out "$OUT_DIR/crystal-sheet.png"

    echo "Done:"
    ls -1 "$OUT_DIR"
    exit 0
fi

Build

# Default the output into OUT_DIR when the caller did not pass --out.
if [[ " $* " != *" --out "* ]]; then
    Render "$@" --out "$OUT_DIR/preview.png"
else
    Render "$@"
fi
