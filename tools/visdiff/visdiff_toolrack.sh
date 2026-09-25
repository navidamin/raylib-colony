#!/usr/bin/env bash
# ToolRack half of the visual diff (spec 5). Grain is OFF on both sides --
# makeGrain seeds from Math.random(), so a grained region cannot diff stably.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
OUT="${OUT:-build/visdiff}"; mkdir -p "$OUT"
LEVEL="${LEVEL:-6}"; TAG="${TAG:-rack}"; SS="${SS:-2}"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
# js/toolrack.js is dashboard.html 21-709 extracted verbatim so the reference
# page can load the module alone. Regenerate after any change to the source:
#   python3 -c "l=open('js/dashboard.html').read().split(chr(10)); \
#     open('js/toolrack.js','w').write(chr(10).join(l[20:709]))"
[ -f build/CMakeCache.txt ] || cmake -B build >/dev/null
# A silent build failure means the diff measures a STALE binary. It cost a
# gamma sweep in which four different exponents all produced byte-identical
# output, because none of them had compiled.
cmake --build build --target toolrack_visdiff -j"$(nproc)" || { echo "BUILD FAILED"; exit 4; }
NODE_PATH=/opt/node22/lib/node_modules node tools/visdiff/shoot_toolrack.js \
  out="$OUT/ref_$TAG.png" level="$LEVEL"
xvfb-run -a -s "-screen 0 1920x1400x24" ./build/tools/toolrack_visdiff \
  --level "$LEVEL" --ss "$SS" --out "$OUT/port_$TAG.png"
python3 tools/visdiff/diff.py "$OUT/ref_$TAG.png" "$OUT/port_$TAG.png" "$OUT/heat_$TAG.png"
