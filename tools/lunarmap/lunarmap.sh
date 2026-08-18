#!/usr/bin/env bash
#
# Render real-elevation lunar maps headlessly (xvfb + software OpenGL).
#
#   tools/lunarmap/lunarmap.sh --nearside
#   tools/lunarmap/lunarmap.sh --pick 43.3,17.3 --span 250
#   tools/lunarmap/lunarmap.sh --playtest      # the whole review suite
#
# Other flags are forwarded to lunar_map (see --help).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/build/lunarmap}"
BINARY="$BUILD_DIR/src/lunar_map"

cd "$REPO_ROOT"
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

Build()
{
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" >/dev/null
    fi
    cmake --build "$BUILD_DIR" --target lunar_map -j"$(nproc)" >/dev/null
}

Render() { xvfb-run -a -s "-screen 0 1920x1200x24" "$BINARY" "$@"; }

mkdir -p "$OUT_DIR"
Build

if [ "${1:-}" = "--playtest" ]; then
    # The review suite: the near side both ways, then famous ground at
    # regional scale. Real coordinates — Tycho, Copernicus, Aristoteles,
    # Mare Imbrium (the game's default anchor), and the south pole.
    Render --nearside --style shaded --out "$OUT_DIR/nearside_shaded.png"
    Render --nearside --style color  --out "$OUT_DIR/nearside_color.png"
    Render --pick -43.3,-11.4 --span 200 --out "$OUT_DIR/tycho.png"
    Render --pick -43.3,-11.4 --span 200 --tilt --out "$OUT_DIR/tycho_tilt.png"
    Render --pick 9.6,-20.1 --span 200 --out "$OUT_DIR/copernicus.png"
    Render --pick 43.3,17.3 --span 250 --out "$OUT_DIR/aristoteles.png"
    Render --pick 32.8,-15.6 --span 300 --out "$OUT_DIR/mare_imbrium.png"
    Render --pick 32.8,-15.6 --span 300 --style color \
           --out "$OUT_DIR/mare_imbrium_color.png"
    Render --pick -88.5,0.0 --span 300 --sun 90,4 \
           --out "$OUT_DIR/south_pole.png"
    ls -1 "$OUT_DIR"
    exit 0
fi

if [[ " $* " != *" --out "* ]]; then
    Render "$@" --out "$OUT_DIR/lunarmap.png"
else
    Render "$@"
fi
