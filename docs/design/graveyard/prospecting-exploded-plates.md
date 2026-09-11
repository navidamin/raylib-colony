# The four exploded plates (prospecting's block model)

**Lived:** `src/Engine/rendermanager.cpp` — the plate stack and its analytic
pick inside `DrawProspectingPanel`, drawn through `DrawBlockLayer` and picked
through `src/Prospecting/block_pick.h`
**Removed:** survey console port, C1 — the block becomes one solid body
**Replaced by:** `src/Survey/survey_block.cpp`

## What it was

Prospecting's model of the ground: four horizontal isometric plates, one per
depth layer, floating one above the other with a gap between them. Each plate
was the whole 32x32 lattice at that layer, each cell **lifted** by its believed
grade and **tinted** by its resource class, so a plate read as a relief map of
one stratum. The plate under the pointer came up to full brightness and the
other three rested dim; the plate the bit was cutting rim-lit and pulsed.

You drilled it with two clicks: one on a cell of the surface plate to collar
the hole, one on a cell of a lower plate to say how deep it should go — which
also set the hole's **deviation**, because the two cells need not be in the
same column. Picking was analytic, inverting the iso transform per plate
against the lifted surface rather than the plate's base plane (`block_pick.h`,
and the round trip is still under test there).

## Why it went

The design it served was superseded. `survey-dashboard-design.md` is the plan
of record for the module's presentation, and its block is **one solid body cut
into four beds**, turned in yaw and pitch — not four separated planes seen from
a fixed isometric camera. The two cannot both be on screen; one of them had to
go, and the exploded stack is the one that answers fewer questions:

- **It could not show a boundary.** Four plates show four surfaces. What a
  geologist reads off a block is the *contact* between beds — how thick a unit
  is, whether it rolls, whether it truncates the one under it — and separated
  plates have no contacts to read.
- **Height already meant grade**, so it could not also mean depth. That is why
  the plates needed a depth axis drawn beside them in the dock: the picture
  could not carry its own vertical scale.
- **Four plates of data is more than anyone reads at once.** The brightness
  easing (`ProspectingSystem::UpdatePlateLight`) existed to hide three of them,
  which is a good fix for a problem the solid block does not have.
- **The camera was fixed.** A hole in the far corner improves the whole model,
  and there was no way to go and look at the far corner.

## What survived

Most of it, and in better shape:

- **One picture of the ground, shared with excavation.** The plates were
  prospecting's; `src/Survey/` is owned by neither module, because the two dig
  the same rock and must never disagree about it.
- **The analytic pick.** The lesson of `block_pick.h` — invert against the
  surface the player can SEE, not against a base plane, or the answer is off by
  the relief — is exactly the lesson the solid block's ray march encodes, and
  for the same reason. `block_pick.h` itself is still live: excavation's panel
  still draws plates and still picks them this way.
- **Focus by brightness** became focus by **isolate**: tap a bed and it stays
  solid while the others ghost. `UpdatePlateLight` is still called and still
  eases; what it lights has changed.
- **`DrawBlockLayer` is not dead.** Excavation's panel still draws its lattice
  as a plate, and it still uses this function and these textures. This record
  is about prospecting's stack of four, not about the plate primitive.

## What would bring it back

Nothing, as prospecting's main view. One thing it did is still missing and is
owed a home rather than a resurrection: **deviated holes**. Two clicks on two
plates in different columns was a genuinely good way to aim a slanted hole, and
the solid block has no equivalent — its second click is a depth, so every hole
is vertical. If deviation earns its place back as a mechanic, it needs a
control on the borehole bar (an azimuth and a dip), not a second plate to click.
