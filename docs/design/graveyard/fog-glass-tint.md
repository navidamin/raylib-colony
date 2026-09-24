# The glass tint on unknown faces

**Lived:** `src/ui/holo3d.c` — the `glass` fill in `h3d_paint_band`, and its
`glassA` argument (0.30 on walls, 0.45 on the cutaway's faces)
**Removed:** the change that closed the cut after a hole, the same day
**Replaced by:** nothing on the faces; the cut's stronger edges

## What it was

Under the fog, unknown rock was pure wire. To make the cutaway's notch read
at zero holes, the unknown part of every face got a faint neutral steel tint,
lit like its face, a little stronger on the cut faces.

## Why it went

It made the cut readable and the block confusing: together with bands faded
in over the whole confidence range, it produced translucent sheets that read
as the surface continuing into the volume. The player's words: "no need for
the transparent surface continuation, it just adds confusion".

## What survived

The stronger cut edges it came with, which now carry the notch alone; and
the rule it taught -- fog coverage is a short step (drawn, or wire), not a
long fade.

## What would bring it back

A view where the cut must read with its edges hidden -- a very low tilt,
where the cut edges collapse onto each other. Then a tint on the cut faces
only, never the walls.
