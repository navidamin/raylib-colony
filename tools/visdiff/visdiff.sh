#!/usr/bin/env bash
# Side-by-side diff of the JS reference and the raylib port at identical
# design size and identical fixed state (spec 5). Target: under 2%.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
OUT="${OUT:-build/visdiff}"; mkdir -p "$OUT"
YAW="${YAW:--0.1}"; PITCH="${PITCH:-0.42}"; SEL="${SEL:--1}"; EXPLODE="${EXPLODE:-0}"
T="${T:-3.0}"; HUD="${HUD:-1}"; TAG="${TAG:-home}"; SS="${SS:-1}"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
# configure only if the cache is missing: reconfiguring raylib every run cost
# more than the diff itself
[ -f build/CMakeCache.txt ] || cmake -B build >/dev/null
# A silent build failure means the diff measures a STALE binary. It cost a
# gamma sweep in which four different exponents all produced byte-identical
# output, because none of them had compiled.
cmake --build build --target holo3d_visdiff -j"$(nproc)" || { echo "BUILD FAILED"; exit 4; }
NODE_PATH=/opt/node22/lib/node_modules node tools/visdiff/shoot.js \
  out="$OUT/ref_$TAG.png" yaw="$YAW" pitch="$PITCH" sel="$SEL" explode="$EXPLODE" t="$T" hud="$HUD"
xvfb-run -a -s "-screen 0 1920x1400x24" ./build/tools/holo3d_visdiff \
  --yaw "$YAW" --pitch "$PITCH" --sel "$SEL" --explode "$EXPLODE" --t "$T" --hud "$HUD" \
  --ss "$SS" --out "$OUT/port_$TAG.png"
python3 tools/visdiff/diff.py "$OUT/ref_$TAG.png" "$OUT/port_$TAG.png" "$OUT/heat_$TAG.png"
