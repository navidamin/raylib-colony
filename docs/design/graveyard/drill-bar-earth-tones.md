# The drill bar's earth tones

**Lived:** `src/ui/drill_sim.{h,c}` -- the `col`, `edge` and `grain` triplets
on each `DrillStratum` (redline.html's strata: browns for the regolith beds,
slate for FRACTURED, charcoal for INTACT BASALT). Read by the drill bar's
well, the core card's strip and the cuttings. Also `GROUND_COLOURS` in
`holo3d.c`, and the literal table behind `SURVEY_BED_PALETTE` in
`src/Survey/survey_constants.h`.
The same browns lived on in `rendermanager.cpp` as `DP_ROCK_COL`,
`DP_ROCK_EDGE` and `DP_ROCK_GRAIN`. They coloured the excavation shaft dock,
the block-model plates and the old borehole dock, with a copy in the preview's
strata sheet. They went in the console fix plan's step 7, to `BED_ROCK`,
`BED_DEEP` and `BED_MID`.
**Removed:** the change that gave the beds one palette
**Replaced by:** `src/ui/bed_palette.h`

## What it was

The drill bar came from the redline prototype with its own rock colours:
dark, low-saturation earth tones meant to sit behind a bright steel auger.
The block had its own table, and the C++ survey block a third copy of the
block's old blues.

## Why it went

The block was repainted "ice & iron" and the drill bar stayed brown, so the
same bed read as two different rocks side by side. The player's words: "the
drill bar colors should be coherent with the block layer colors. make it so
that they are entangled in case of changes in future". Three tables could
only ever agree by someone remembering to edit all three.

## What survived

The reason the earth tones were dark. The well is still darker than the
block's faces (each bed at x0.34), so the steel string and the bed names
read on it. The cuttings are still a lighter tone of their rock.

## What would bring it back

Nothing that brings back a separate table. If the well ever needs a
different look from the block, derive it from bed_palette.h as another
shade, as `BED_WELL` is.
