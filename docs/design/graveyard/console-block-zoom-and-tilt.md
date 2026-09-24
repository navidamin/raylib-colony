# The console block's zoom and tilt

**Lived:** `src/ui/survey_dash.c` -- `SurveyDash_Zoom` (wheel notches,
`DASH_ZOOM_MIN/MAX/STEP` 0.15 / 1.30 / x1.12 per notch, anchored under the
pointer), the pitch term of `SurveyDash_Drag` (`dy x 0.004`, clamped to
`DASH_PITCH_MIN/MAX` 0.15-0.80 rad), and the wheel call in
`RenderManager`'s console input
**Removed:** the change that made the block turn about its vertical axis only
**Replaced by:** a fixed view -- pitch 0.42, zoom 0.37 -- and a yaw-only drag

## What it was

The reference (`Holo3D.attach`) let a drag turn the block both ways and the
wheel zoom it. The port kept both. An earlier playtest had already found the
tilt too free -- near top-down the column collapsed, near edge-on the cap
vanished -- and it was clamped to a band rather than removed.

## Why it went

The player's words: "right now the block is zoomable and rotatable in all
directions. make it rotatable only around the z axis. and not zoomable."
Everything the console now draws on the block -- the height log, the
stretched borehole, the cutaway and its reveal -- is read at one tilt and
one size. Three ways to move the view were three ways to lose what was being
read, for no decision the player needed them for: the block is one sect's
ground, it always fits the pane, and turning it is enough to see behind the
cut.

## What survived

The drag, as yaw. The rule the clamped tilt taught: the tilt is chosen for
the cap and the column to both read, and 0.42 is that choice.

## What would bring it back

A block too large or too detailed to read at one size -- many holes packed
tight, a finer lattice -- would want zoom back, as a pinch or wheel anchored
under the pointer, as it was. Tilt would need a reason the cutaway cannot
give: a feature that only reads from near top-down.
