# Resource Distribution Model

> Status: STUB
> Last Updated: 2026-04-04
> Parent: [prospecting-master-design.md](prospecting-master-design.md)
> Schedule: **Sub-cell generation needed for core pipeline.** Pathfinder/clue chaining deferred to future phase.

---

## Purpose

Define how resources are distributed across the planet grid with geological coherence, supporting:
1. **Sub-cell distribution** (core pipeline requirement — see below)
2. Pathfinder mechanics (indicator elements → target deposits)
3. Clue chaining (spatial trails between related cells)
4. Depth distribution (what's at each layer)
5. Cross-referencing (adjacent cells sharing geological features)

## Sub-Cell Distribution (Required for Core Pipeline)

The prospecting grid **subdivides a single planet cell** into NxN sub-cells (3×3 at T0, up to 6×6 at T3). The `ResourceManager` stores one abundance value per resource per planet cell. Sub-cell distribution determines how that abundance is spread within the cell at higher resolution.

### Requirement

Given a planet cell with resource abundance values (e.g., Fe=0.7, Si=0.3), generate an NxN sub-cell grid where:
- Sub-cell values average to approximately the planet cell's value
- Distribution has spatial coherence (clusters/gradients, not random noise)
- Deposits form recognizable shapes (see ore shape templates in [ui-layout.md](ui-layout.md))
- Distribution is deterministic (same seed + same cell = same sub-cell map)
- Works at any grid size (3×3 through 6×6) — finer grids reveal more detail within the same spatial area

### Interaction with Existing Systems

```
Planet cell (ResourceManager)     Prospecting sub-cells (new)
┌────────────────────┐            ┌──┬──┬──┬──┬──┐
│                    │            │.2│.3│.1│.4│.2│
│  Fe = 0.7          │  ──gen──► │.3│.8│.9│.5│.3│  avg ≈ 0.7
│  Si = 0.3          │            │.1│.7│.5│.4│.2│
│  (coarse, known)   │            │.2│.3│.6│.8│.4│
│                    │            │.1│.2│.3│.2│.1│
└────────────────────┘            └──┴──┴──┴──┴──┘
```

- `OrbitalSurveyData` and `SiteArchetype` remain per planet cell (coarse view)
- Sub-cell data is generated on-demand when a prospecting unit first operates on a cell
- Sub-cell map is seeded from planet seed + cell coordinates (deterministic)
- Depth layers (see [depth-sampling-design.md](depth-sampling-design.md)) apply per sub-cell

[?] Implementation approach: Perlin noise? Voronoi clusters? Simple gradient + noise? Needs prototyping.

## Current System

`ResourceManager` (src/ResourceManager/) generates resources using cluster-based procedural generation:
- Each grid cell gets resource abundances (0.0-1.0) per ResourceType
- Clusters create spatial grouping but cells are otherwise independent
- No geological coherence model (no layer relationships, no pathfinder correlations)

[?] — Need to assess: how much spatial correlation exists in current generation? Are adjacent cells already related, or fully independent?

## Required Enhancements

### 1. Geological Coherence Model

Adjacent cells should share geological features. Deposits should form continuous bodies, not random per-cell noise.

[?] Options:
- **Gradient-based:** Each resource has a smooth gradient across the grid (Perlin noise or similar)
- **Vein-based:** Linear deposit structures that cross multiple cells
- **Layer-based:** Geological strata that span areas with consistent depth profiles
- **Hybrid:** Combination (gradients for bulk minerals, veins for rare elements)

### 2. Pathfinder Correlation Rules

Define which elements indicate which target deposits:

| Indicator Element | Target Deposit | Correlation Type |
|-------------------|---------------|-----------------|
| [?] | [?] | [?] |
| [?] | [?] | [?] |

Real-world examples to adapt:
- Arsenic, antimony, bismuth → gold
- Chromium → platinum group
- Barium → lead/zinc
- Fluorine → rare earth elements

[?] — Need lunar/planetary-appropriate correlations (not all terrestrial pathfinders apply)

### 3. Depth Distribution Rules

See [depth-sampling-design.md](depth-sampling-design.md) for layer structure.

[?] — How does depth distribution interact with lateral (grid) distribution?

### 4. Clue Chaining Spatial Rules

How do deposits cluster to support meaningful trail-following?

[?] — Options:
- Deposits form elongated shapes (player follows the long axis)
- Gradient peaks with shoulders (trail follows gradient uphill)
- Vein structures with branching (player follows veins)

## Resource Map Initialization

[?] — When and how is the resource map generated?
- At planet creation (fixed seed)?
- Does the map include depth information from the start, or is depth generated on-demand when sampled?
- How does this interact with `OrbitalSurveyData` and `SiteArchetype`?

## Open Questions

- Should the resource map be deterministic (same seed = same map) or have randomized elements?
- How does map generation scale with planet size changes?
- Should there be "barren" cells with genuinely nothing, or always something?
- How do geological events (volcanic, impact) affect distribution?
