# The flat per-cell fallback skim

**Lived:** `src/Unit/unit.cpp:1402-1426`, with its rate table at `1347-1353`
**Removed:** the commit carrying this record — *Phase 0: bury the excavator model the live system already replaced*
**Replaced by:** nothing — `ExcavationSystem::Dig` is now unconditional

## What it was

The `else` arm of `ProcessExtraction`'s Stage 1. When a unit had no
`ExcavationSystem` or no `ProspectingSystem`, it skimmed the parent cell evenly
instead of digging a spot: for each resource present, take
`baseRate × efficiency × tier × abundance × ops × directive × scan ×
priorityBoost × dt × excavatorCount`. `baseRate` came from a `std::map` built
out of `parameters["FeExtractionRate"]` and friends.

## Why it went

**It could not be reached.** Both systems are constructed together in the `Unit`
constructor for every Extraction unit, so `excavationSystem && prospectingSystem`
is true whenever `ProcessExtraction` runs. The comment claimed it covered "older
saves, harnesses" — there is no save format, and every harness in `tools/`
builds a real unit.

Left in place it did active harm: it was the last consumer of the
`extractionRates` map and of `availableResources`, which in turn was the last
consumer of the dead depth derivation. One unreachable branch was keeping three
other dead things looking alive.

It also encoded the model the module exists to replace. Skimming a whole
100 m cell evenly is precisely what `excavation-design.md` argues against — if
the ground has no spatial structure worth aiming at, then surveying is worthless
and the module has no decision in it.

## What survived

**Every modifier in the chain.** Operations efficiency, the directive
modifier, module tier and survey gating all still multiply into Stage 1 — they
are folded into the single `externalMultiplier` handed to
`ExcavationSystem::Dig`, keeping their old meanings. The `PRIORITIZE` directive
survived in a better form: it used to be a flat +40% on one resource, and now
steers `targetResource`, which is what a player would have meant by it.

## What would bring it back

A second kind of extraction unit that genuinely has no prospecting grid under it
— an orbital skimmer, a mobile harvester. That is a different unit type with its
own process method, not a defensive `else` inside this one.
