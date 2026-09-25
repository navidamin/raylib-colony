#!/usr/bin/env bash
# DomeForge port gate: the prototype's JS against src/DomeForge/, same config,
# every kind the game uses. Pass: every case under 1% differing pixels.
#   tools/domeforge/domeforge_diff.sh            # the standard set
#   tools/domeforge/domeforge_diff.sh --kind unit --size 180 --color '#8a8a8a'   # one case
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
OUT="${OUT:-build/domeforge}"; mkdir -p "$OUT"
export PATH="/opt/node22/bin:$PATH"
[ -f build/CMakeCache.txt ] || cmake -B build >/dev/null
# A silent build failure means the diff measures a stale binary: fail loudly.
cmake --build build --target domeforge_render -j"$(nproc)" >/dev/null || { echo "BUILD FAILED"; exit 4; }
BIN=build/tools/domeforge_render

run() {   # tag, then the flags both sides share
  local tag="$1"; shift
  node tools/domeforge/ref.js "$@" --out "$OUT/ref_$tag.png" >/dev/null || { echo "REF FAILED: $tag"; return 3; }
  "$BIN" "$@" --out "$OUT/port_$tag.png" | sed "s/^/  /"
  printf '%-22s ' "$tag"; python3 tools/domeforge/diff.py "$OUT/ref_$tag.png" "$OUT/port_$tag.png" "$OUT/heat_$tag.png"
}

if [ $# -gt 0 ]; then run custom "$@"; exit $?; fi
fail=0
run unit_default          --kind unit                                                   || fail=1
run unit_two_sockets_off  --kind unit --size 180 --color '#8a8a8a' --socket-start 225 --socket-count 2 || fail=1
run central_default       --kind central                                                || fail=1
run roads_half            --kind roads --scale 0.5                                      || fail=1
run ground_quarter        --kind ground --scale 0.25                                    || fail=1
run base_half             --kind base --scale 0.5                                       || fail=1
[ $fail -eq 0 ] && echo "ALL UNDER GATE" || echo "GATE FAILED"
exit $fail
