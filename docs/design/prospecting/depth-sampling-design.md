# Depth Sampling Design

> Status: STUB
> Last Updated: 2026-04-01
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define how resources are distributed across depth layers, how tier progression gates depth access, and how the default allocation priority works vs player-optimized depth selection.

## Layer Structure

| Layer | Index | Tier Required | Default Auto Priority | Resource Profile |
|-------|-------|--------------|----------------------|-----------------|
| Surface | 0 | T0 | 1st | [?] |
| Shallow | 1 | T1 | 2nd | [?] |
| Mid | 2 | T2 | [?] | [?] |
| Deep | 3 | T3 | [?] | [?] |

## Resource Distribution by Depth

[?] — Key design decisions needed:

- Which resources concentrate at which depths?
- Is distribution deterministic (always iron at surface) or procedural (varies by cell)?
- How does depth distribution interact with the planet-level resource map in `ResourceManager`?
- Are there depth-exclusive resources? (e.g., water ice only at depth 2-3, He-3 only at depth 3)

### Proposed Model (rough)

```
Surface:  Common bulk minerals (Fe, Si, Al, Ca) — easy access, moderate abundance
Shallow:  Refined minerals (Ti, alloys precursors) — moderate access, variable
Mid:      Volatiles (H2, H2O) — harder access, high value when found
Deep:     Rare elements (He-3, trace minerals) — expensive access, very high value
```

[?] This model needs validation against gameplay balance.

## Default vs Fine-Control

**Default allocation priority:** The auto-mode samples depths in a fixed order. The proposed order (1st → 2nd → skip → 4th) creates an incentive for player intervention — the skipped mid-layer contains volatiles that are valuable but overlooked by default.

[?] Alternative: default could simply go 1→2→3→4 but with lower sample quality at each depth.

## Drilling Cost Scaling

[?] How does cost scale with depth?

| Option | Formula | Gameplay Effect |
|--------|---------|----------------|
| Linear | cost = base * (depth + 1) | Predictable, simple |
| Exponential | cost = base * 2^depth | Deep drilling is a significant investment |
| Stepped | fixed costs per tier | Simplest, least interesting |

## Open Questions

- Can player re-sample same cell/depth? If yes, does it improve data or just waste resources?
- Does drilling disturb adjacent layers? (sample quality penalty if already drilled nearby?)
- Are there geological formations that span multiple layers? (veins, fault lines)
- How does depth interact with stratigraphic column visualization (7E)?
