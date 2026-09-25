# The console block's smooth blue cap

**Lived:** `src/ui/holo3d.c`, the `k == 0` branch of the top-surface painter
in `h3d_paint_layer`. It drew real ground's cap as slope-shaded quads mixed
between two fixed blues (`#123f6e` → `#3f92d0`), with a 30% mesh grid over
it. It was sampled on the reference's 16 × 13 grid. The ground under it had
three small craters (`SURVEY_CRATER_N` 3, radius 0.028–0.07, bowl 4.5% of
the column) that mostly fell between those samples.
**Removed:** the regolith-cap change
**Replaced by:** `h3d_paint_regolith_cap`, on a 32 × 32 grid, over a ground
with eight craters and surface rolls

## What it was

The top of the block, as the reference drew it: a smooth, gently shaded
blue sheet with a wire grid. It said "the surface" and nothing about what
the surface is.

## Why it went

The player's words: "It should be actual regolith layer with craters and
ups and downs, but pixelated and low poly." A blue sheet reads as water or
glass. And its craters were real in the ground data (visible in a height
dump) but invisible on screen: too small for the grid, and shaded with too
little contrast to show.

## What survived

The ground model is still the source of the shape. The new cap draws
exactly the surface the walls, the cutaway, the drill and the hit tests
use. The mesh and slope logic survive for the lower beds' tops when the
block is exploded. The reference path (no ground) still has the blue cap,
because the port's visual diff measures it.

## What would bring it back

Nothing for real ground. A reference-faithful mode for comparing against
the JS would use the reference path, which never lost it.
