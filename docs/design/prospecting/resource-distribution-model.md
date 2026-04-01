# Resource Distribution Model

> Status: STUB
> Last Updated: 2026-04-01
> Parent: [prospecting-master-design.md](prospecting-master-design.md)
> Schedule: Future phase (required for pathfinder and clue chaining mechanics)

---

## Purpose

Define how resources are distributed across the planet grid with geological coherence, supporting:
1. Pathfinder mechanics (indicator elements → target deposits)
2. Clue chaining (spatial trails between related cells)
3. Depth distribution (what's at each layer)
4. Cross-referencing (adjacent cells sharing geological features)

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
