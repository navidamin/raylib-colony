# Excavator controls — `MoveExcavator` / `SetExcavatorDepth` / `SetExcavatorRate` / `GetExcavators`

**Lived:** `src/Unit/unit.h:127-130`, `src/Unit/unit.cpp:1780-1825`
**Removed:** the commit carrying this record — *Phase 0: bury the excavator model the live system already replaced*
**Replaced by:** `src/Excavation/excavation_system.h` — `selectedSpotX/Y`, `selectedDepth`, `activeMachine`, `pace`

## What it was

The player-facing API of the original excavation model: move an excavator to a
grid cell, set how deep it digs (clamped by a per-tier table `{10, 30, 100,
300}` centimetres), set its rate in kg/hr, and read the fleet back.

## Why it went

**Nothing ever called them.** Not the renderer, not the input layer, not the
tools, not the tests — a grep of `src/`, `tools/` and `tests/` returned the
declarations and the definitions and nothing else.

That is worse than idle code, because one of them was load-bearing by its
absence. `Excavator::depth` is only ever written by `SetExcavatorDepth`, so it
stayed `0.0f` for the life of every unit, which pinned
`ProcessExtraction`'s depth-layer derivation to `SURFACE` forever (buried
separately in [`extraction-depth-from-excavator.md`](extraction-depth-from-excavator.md)).
A dead setter was silently deciding what depth the game dug at.

The replacement arrived without them being removed: `ExcavationSystem` holds
selection, depth and machine choice as public state that the panel reads and
writes directly, so the `Unit`-level API had nothing left to mediate.

## What survived

The **shape of the controls**, all of it. Position, depth, rate and machine are
still the four things a player sets — they moved from four setters on `Unit` to
plain fields on the module's own facade, which is where CLAUDE.md says module
state belongs. The per-tier depth ceiling survived too, as
`EXC_MAX_DEPTH_PER_TIER` in `excavation_constants.h`, but in **layer indices
`{1,2,3,4}` rather than centimetres** — the centimetre story
(`{10, 30, 100, 300}` cm) had already lost to the metric column model
(0–12 / 12–34 / 34–68 / 68–120 m) everywhere else.

## What would bring it back

Nothing in this shape. If excavators ever become individually placeable again —
a fleet you position rather than a machine you pick — that is a new design with
per-machine state on `ExcavationSystem`, not four setters on `Unit`.
