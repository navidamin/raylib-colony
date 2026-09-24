# The section slice over the block

**Lived:** `src/ui/survey_dash.c` — `DashDrawSection` and `DASH_SECTION_N`
**Removed:** the cutaway change, the day after it shipped
**Replaced by:** `Holo3D_DrawCutaway` in `src/ui/holo3d.c`

## What it was

Once a site was taken, a flat vertical panel was drawn over the block through
the site, cut along the screen's horizontal so it faced the viewer: a dark
backing, each bed filled between the wall's lit and mid tones, each bed's top
interface stroked in the wall's line colour, two pale end lines. The depth
line ran down it.

## Why it went

It measured right and read wrong. The problem it solved was real -- the
camera's tilt makes a point deep inside the block draw higher the further
back it sits, so a depth line from a site near the back ended up to half a
column above the same depth on the front wall -- and against the slice the
line did sit in the right bed. But the slice was drawn ON the block, not OF
it: its own flat fill, its own edges, a rectangle floating in front of walls
it was supposed to be part of. The player's words: "a new rectangle put on
top, irrelevant to everything else".

## What survived

- **The diagnosis**, and the fact that limiting tilt cannot fix it.
- **The read-only Holo3D API** it needed: `Holo3D_BedSpan`,
  `Holo3D_LayerCount`, `Holo3D_Layer`. The cutaway is built on the same
  bed spans, and the tag's bed name (`DashBedAt`) still uses them.
- **The tag naming the bed** at the chosen depth.

`Holo3D_ScreenAcross` is still there but nothing calls it now.

## What would bring it back

A view where cutting the block is not possible -- a top-down map or an
exploded block, where there is no solid to cut. There a section panel is the
honest way to show the beds under a point, and it should then be drawn as a
separate inset, not over the model.
