# `DEFAULT_WearAndTear` / `parameters["WearAndTear"]`

**Lived:** `src/game_constants.h:96`, written at `src/Unit/unit.cpp:242`
**Removed:** the commit carrying this record — *Phase 0: bury the excavator model the live system already replaced*
**Replaced by:** `DigResult::wearDelta` and `EXC_MACHINES[].wear`

## What it was

A float, `0.2`, stored into every unit's generic `parameters` map at
construction.

## Why it went

Written once, read never. It predates wear being modelled at all.

Wear is now three concrete things and none of them is a unit-level constant: a
per-machine multiplier in the machine table (`EXC_MACHINES[].wear`, where the
bucket drum's `0.4` against the hammer's `2.0` is a real trade the player
chooses between), a per-tick `DigResult::wearDelta` produced by the work done,
and a per-bit fatigue ladder in the drill constants.

## What survived

The idea that wear is a **stat that differs per machine**, which is now the
thing that makes the machine table a choice rather than a ladder. What did not
survive is wear as one number for a whole unit.

## What would bring it back

Nothing. Per-unit wear, if it is ever wanted, is a property of the unit's
structure and belongs on `Unit` as a typed field, not as an untyped entry in a
string-keyed parameter bag.
