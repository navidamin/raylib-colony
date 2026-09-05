# The flat excavation lattice, and its four cell helpers

**Lived:** `src/Engine/rendermanager.cpp` — the lattice inside `DrawExcavationPanel`,
plus `ProsDrawCellBase`, `ProsDrawLockedCell`, `ProsDrawWorkedMark`,
`ProsDrawCellMarker`, `ExcYieldHeatColor` and `PROS_SELECT_BORDER`
**Removed:** the commit carrying this record — *Phase 2: excavation reads the ground as a block model*
**Replaced by:** the four-plate iso stack — `ProsDrawBlockLayer` fed by `BuildEstimateField`

## What it was

Excavation's view of the ground: a flat N×N grid of rounded rectangles, one per
sub-cell, shaded by a slate→green ramp over the estimated yield of the targeted
resource, with a separate four-button DEPTH row underneath to say which layer
you were looking at. Out-of-reach cells got a dashed border and a lock glyph on
hover; dug cells got a hatched amber corner wedge; sampled cells got a ring in
the middle.

## Why it went

**The lattice doubled twice and the drawing did not survive it.** At 8×8 the
cells were finger-sized and every marker in the list above was legible. At 32×32
a cell is about 3 px across in the panel's width — smaller than the 2 px border
the base drew around it. The lock glyph, the worked wedge, the confidence ring
and the selection outline had all become sub-pixel decoration on a field of
1,024 dots, and the panel read as texture rather than as ground.

The block model answers the same questions in a form that scales, because it
puts everything in **colour and lift** rather than in per-cell furniture:
estimated grade is height, confidence class is hue, and a worked-out spot simply
sinks. That last one is the honest replacement for the amber wedge — relief now
reads as *what is left* rather than what was once there, which is the question
an excavation panel is actually asking.

The depth row went with it for a better reason than space. **The plate is the
depth.** Clicking a cell on the MID plate says both "this spot" and "this layer"
in one gesture, where before those were two controls that had to be kept in sync
— a class of bug the panel no longer has room for.

One thing was lost and deliberately not rebuilt: the wedge distinguished
"already worked out" from "always poor". It turned out not to need drawing.
Digging sets confidence to 1.0 at that spot and depth, so a worked-out cell is a
MEASURED cell with no relief while barren unsurveyed ground is UNCLASSIFIED with
no relief. **The class colour already carried it.**

The four `Pros*` cell helpers were only ever called by this lattice despite the
prefix — prospecting stopped using them when its own panel became the block
model. They died with their one caller, along with `ExcYieldHeatColor` and
`PROS_SELECT_BORDER`.

## What survived

- **Every question the lattice answered.** Which spot, which depth, how good, how
  certain, how worked, what is in reach — all of it, in the plate stack.
- **The reach ring**, promoted from a per-cell dashed border to one dashed
  boundary square per plate. A boundary is a boundary; drawing it 1,024 times
  was the mistake.
- **The locked state**, promoted from a per-cell glyph to a whole plate held
  back at 0.42 light with `LOCKED` on its label — a depth is locked, not a cell.
- **The out-of-reach tooltip**, unchanged, now fed by the plate hover instead of
  the grid hover.

## What would bring it back

A lattice small enough for per-cell furniture to read — 8×8 or so. The grid went
8 → 16 → 32 for reasons the prospecting design records, and nothing suggests it
is going back. If a future panel needs to show a handful of spots rather than a
field of them, that is a different widget than this was.
