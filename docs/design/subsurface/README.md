# Subsurface — Design Documents

> **Auto-context rule:** Read this README before touching resource generation
> (`ResourceManager::GenerateResourceMap`, `ProspectingGrid::GenerateLayerDistribution`),
> or before changing anything that both prospecting and excavation read.

This directory is not a module. It is the **world both modules read**, and the
contract between them. It exists because the ground had no design document of
its own — it was described in two places, differently, and the code did a third
thing.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [subsurface-model.md](subsurface-model.md) | **What is actually down there.** 3D shoots with orientation, geometry and units, what changes in the generator | PRELIMINARY |
| 2 | [module-interplay.md](module-interplay.md) | **The contract**, and the empty-out effect of mining on the model | DESIGN |
| 3 | [drilling-procedure.md](drilling-procedure.md) | **The machine-ground contract.** Heat, pressure, wear, cuttings and telemetry as felt mechanics, for both modules; where learning lives; failures cost knowledge (prospecting) or product (excavation) | DESIGN |

## The One-Paragraph Version

The ground is made of **shoots**: 3D ellipsoids of elevated grade with a
position, three radii, and an *orientation*. Orientation is the whole point —
a dipping shoot is what makes "which way do I point the drill" a real question,
and without it an angled hole is never worth more than a vertical one. Each
element gets its own shoots, so iron and water lie in different places at
different angles in the same ground.

Prospecting **fills the model in**; excavation **empties it out**. They meet at
one mutable field, `SubCell::workedFraction`, and one setter, `RecordExcavation`
— excavation calls prospecting, never the reverse.

## Why It Needed Writing

The block model design asks the player to learn the shape of an ore body. That
is only worth asking if the ore body *has* a shape.

Today it does not. `resource-distribution-model.md` specifies vertical pillars;
the code generates four independent 2D layers, because depth is in the hash
seed. Both make the central decision of the new prospecting design meaningless,
in opposite directions. See [subsurface-model.md](subsurface-model.md) §1.

## Cross-References

| Where | What |
|-------|------|
| `src/ResourceManager/resource_manager.cpp` | planet-scale abundances per cell per layer |
| `src/Prospecting/prospecting_grid.cpp` | `GenerateLayerDistribution` — the sub-cell field, and the depth-seeded clusters this design changes |
| `src/Prospecting/prospecting_types.h` | `SubCell::workedFraction` — the one mutable field |
| `tools/inspect/measure_clusters.cpp` | `colony_measure_clusters` — run it before and after any generator change |
| [`prospecting/block-model-design.md`](../prospecting/block-model-design.md) | the reader that fills the model in |
| [`excavation/block-mining-design.md`](../excavation/block-mining-design.md) | the reader that empties it out |
