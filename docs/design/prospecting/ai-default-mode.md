# AI & Default Mode Design

> Status: STUB
> Last Updated: 2026-04-01
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define how the AI/automatic mode behaves at each prospecting stage, what efficiency penalties it incurs, and how the player transitions between automatic and fine-control modes.

## Design Principle

Default mode is **functional but suboptimal**. It produces results — the player doesn't HAVE to engage with fine control. But a player who takes control can achieve meaningfully better outcomes through informed decisions.

## Per-Stage Default Behavior

| Stage | Default Behavior | What It Misses | Efficiency Penalty |
|-------|-----------------|----------------|-------------------|
| Sweep | [?] Surface-only, single frequency? | Deep anomalies, frequency optimization | [?] |
| Sampling | [?] Fixed depth priority (L0→L1→L3?) | Optimal depth based on sweep data | [?] |
| Tray/Pipeline | [?] Cheapest separation + cheapest tool? | Element-specific tool matching | [?] |
| Testing | [?] XRF only? Visual + XRF? | Light elements (LIBS), precise values (fire assay) | [?] |

## AI Heuristic Design

[?] — The AI is not just "random defaults." It should have its own logic that makes reasonable but not optimal choices:

### Sweep AI
- [?] Always uses medium frequency?
- [?] Single sweep per area?

### Sampling AI
- [?] Samples highest-anomaly cells first?
- [?] Follows fixed depth order regardless of sweep data?

### Pipeline AI
- [?] Always uses magnetic separation (cheapest)?
- [?] Never uses fire assay (too expensive)?
- [?] Processes in FIFO order?

### Testing AI
- [?] Applies cheapest available tool?
- [?] Cross-references automatically or ignores adjacency?

## Efficiency Penalty Framework

[?] How much worse is default mode?

| Option | Penalty | Feel |
|--------|---------|------|
| Fixed multiplier (e.g., 0.7x) | All results 70% as effective | Simple, predictable |
| Per-stage multiplier | Different penalties per stage | More nuanced |
| Missed opportunities | Full quality but misses some resources | Most realistic |
| Confidence penalty | Same data but lower confidence scores | Ties to confidence system |

## Mode Switching UI

[?] How does the player switch between default and fine-control?

| Option | Description |
|--------|-------------|
| Global toggle | One switch: all-auto or all-manual |
| Per-stage dropdown | Each stage has its own auto/manual selector |
| Implicit | Player interacting with a stage automatically overrides auto for that stage |

## Unlock Progression

[?] When is AI mode available?

| Option | Description |
|--------|-------------|
| Always available | Even T0 has basic auto-mode |
| Tier-gated | AI features unlock at T1/T2/T3 |
| Earned | AI unlocks after player has manually completed N surveys |

## Open Questions

- Should the AI improve over time (learns from player behavior)?
- Can the player customize AI priorities (e.g., "prefer deep sampling")?
- Does AI mode consume more or fewer resources than manual? (more = penalty; fewer = incentive)
- How does AI mode interact with the confidence system?
