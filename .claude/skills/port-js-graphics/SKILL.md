---
name: port-js-graphics
description: Port a JavaScript Canvas 2D graphics model (a .js or .html file that draws with getContext('2d') — gradients, shadowBlur glows, clips, dashed lines, text) into this raylib game through the c2d shim, gated by a pixel diff against the JS. Use whenever the user hands over, drops into js/, links, or pastes JS/HTML graphics, a canvas mockup, a prototype panel or a "JS model", and wants it in the game, reproduced in raylib, or made to "look like the JS". Also use when touching src/ui/, c2d.h, or tools/visdiff/.
---

# Porting JS graphics into raylib

This repo has a purpose-built system for reproducing procedural Canvas 2D
graphics in raylib faithfully: the **c2d shim** (`src/ui/c2d.h`), a
**contract** (`docs/CANVAS2D_PORT_SPEC.md`), and a **visual-diff gate**
(`tools/visdiff/`). The full procedure is
**`docs/guides/js-graphics-port.md`. Read it now, before anything else.**
This file is the checklist that keeps you on it.

## First, confirm it's Canvas 2D

`grep -n "getContext" <file>`. If it's `'2d'`, continue. If it's WebGL,
three.js, SVG or DOM/CSS layout, c2d doesn't apply, so stop and ask the user.

## The steps (details and gates in the guide)

0. **Intake.** Put the source in `js/` verbatim and never edit it. Find the
   canonical file (don't port an extract twice), the line range, the entry
   points the page actually calls, and the design size.
1. **Scan.** `python3 tools/jsport/gapscan.py js/<file> --lines A-B --md`.
   Exit 2 means the shim lacks something.
2. **Inventory.** `docs/design/<module>/<name>-inventory.md`, one row per
   scanned site, every row accounted for, scope decisions and known
   deviations written down. **Show it to the user and wait for approval**
   unless told to run straight through.
3. **Shim first.** Close every `MISSING` row in `c2d.h`/`c2d.c`, each with a
   `tools/c2dtest/c2dtest.c` assertion. Then `tools/c2dtest/c2dtest.sh`
   passes and the existing diffs are unchanged
   (`SS=2 tools/visdiff/visdiff.sh`, `SS=2 tools/visdiff/visdiff_toolrack.sh`).
4. **Harness before port.** Copy the ToolRack set (`ref_toolrack.html`,
   `shoot_toolrack.js`, `toolrack_main.c`, `visdiff_toolrack.sh`, the CMake
   target). Local `@font-face`, fixed time and state, dpr 1, randomness off
   or seeded. Look at the reference PNG.
5. **Port.** `src/ui/<name>.{c,h}`, C99, added to `colony_c2d`. All drawing
   through c2d, coordinates verbatim from the JS, one C function per JS
   function. Port the code, not what you think it meant.
6. **Gate.** `SS=2 tools/visdiff/visdiff_<name>.sh` must come in **under
   2%**. Above that, read the heatmap against the guide's signature table
   **before** changing code.
7. **Wire in.** `c2d_present_into`, hit tests via `c2d_to_design`, check the
   web memory budget, `tools/preview/preview.sh`, and look at the PNG.
8. **Record.** Add a row to the guide's port registry, put the diff numbers
   in the commit message, and add any new trap to the guide.

## Never

- Substitute a flat approximation for any of the twelve gaps (a gradient
  becomes a fill, a glow becomes a line, a partial arc becomes a full
  ellipse).
- Call raylib draw functions from a UI module, or re-derive layout from
  window size, or hit-test in screen space.
- Claim a visual result without rendering it, or call a port done above 2%
  or with an unaccounted inventory row.
- Tune blind against the diff. Read the heatmap. If nothing you change moves
  the number, the build failed and you're measuring a stale binary.
