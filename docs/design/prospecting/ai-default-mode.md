# AI & Default Mode Design

> Status: DRAFT
> Last Updated: 2026-04-04
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define how the AI/automatic mode behaves at each prospecting stage, what efficiency penalties it incurs, and how the player transitions between automatic and fine-control modes.

## Design Principle

Default mode is **functional but suboptimal**. It produces results — the player doesn't HAVE to engage with fine control. But a player who takes control can achieve meaningfully better outcomes through informed decisions.

**Key insight:** The AI's primary penalty is **lack of strategic agency**. It selects cells randomly, uses cheapest tools, and doesn't target based on data patterns or colony needs. The player's advantage is informed decision-making — choosing where to sample based on sweep results, which tools match which elements, and which resources the colony actually needs.

## Availability

AI/default mode is **always available from T0** with basic functionality. Improvements are **purchased with research tokens from the colony pool** — not tied to tier or objectives. AI upgrades apply **globally to all sects**.

### Base AI (Free)
Random cell selection, visual inspection only, stops when tray is full. -20% confidence penalty. Functional but near-useless.

### Research-Gated Upgrades

| Research Project | Cost | What Changes | Prerequisite |
|-----------------|------|-------------|-------------|
| Auto-Collection | Low | AI collects without player input | — |
| Auto-Discard | Low | AI replaces lowest-value sample when full | Auto-Collection |
| Sweep-Guided Targeting | Medium | AI targets sweep-flagged cells | T1 tools |
| Tool Matching | Medium | AI picks XRF/LIBS by element type | T2 tools |
| Multi-Tool Pipelines | Medium | AI runs sequential tools per sample | Tool Matching |
| Context-Aware Presets | High | AI selects preset by site type | Multi-Tool Pipelines |
| Precision Calibration | High | Confidence penalty → 0% | Context-Aware Presets |
| Full Autonomy | Very High | Near-player-quality decisions | All above |

See [prospecting-master-design.md](prospecting-master-design.md) Section 11b for full research tree.

### Empire Scaling

AI research is an empire-management investment. A player with Full Autonomy can set every new sect to Default mode and let AI prospect while they focus elsewhere. A player who skips AI research must manually prospect every sect.

## Per-Stage Default Behavior

| Stage | Default Behavior | What It Misses | Penalty Type |
|-------|-----------------|----------------|-------------|
| Sweep | Surface-only, single frequency | Deep anomalies, frequency optimization | Missed opportunities |
| Sampling | Random cell selection, fixed depth priority (L0→L1→L3→L2) | Targeting based on sweep data, strategic depth selection | Missed opportunities |
| Tray/Pipeline | Cheapest separation + cheapest available tool | Element-specific tool matching, optimal pipeline design | Missed opportunities |
| Testing | Cheapest available tool per tier | Light elements (LIBS), precise values (fire assay) | Missed opportunities |

## AI Heuristic Design

The AI is not random noise — it has its own logic that makes reasonable but not optimal choices.

### Sweep AI
- Always uses surface-only frequency (no deep scanning)
- Single sweep per area (never re-sweeps at different frequencies)
- Covers all cells sequentially (no prioritization)

### Sampling AI
- Selects cells **randomly** from the grid (does not use sweep data to target anomalies)
- Follows fixed depth order: Regolith → Megaregolith → Intact Bedrock → Fractured Bedrock
- Skips fractured bedrock until last (misses water deposits — see [depth-sampling-design.md](depth-sampling-design.md))
- Never re-samples for higher confidence

### Pipeline AI
- Always uses magnetic separation (cheapest available)
- Never uses fire assay (too expensive/destructive)
- Assigns cheapest available analysis tool (Visual at T0, XRF at T1+, LIBS at T2+)
- Processes in FIFO order

### Testing AI
- Applies cheapest available tool to all samples uniformly
- Does not cross-reference adjacent cells
- Does not match tools to element signatures

## Efficiency Penalty Framework

The AI penalty is **primarily missed opportunities** with a **small confidence penalty**:

1. **Missed opportunities** (main penalty): Random cell selection means the AI doesn't target high-value areas revealed by sweep data. Cheapest tools means it misses elements that require specific instruments (e.g., H2O requires LIBS, not XRF). No strategic targeting means it doesn't focus on what the colony actually needs.

2. **Confidence penalty** (secondary): AI-produced results carry a small confidence reduction (max 20% at T0, decreasing to 5% at T3). This represents less careful instrument calibration and sample handling.

**Result:** A player who manually controls prospecting will:
- Find valuable resources faster (targeted sampling)
- Get higher confidence readings (better tool selection)
- Discover depth-specific resources the AI misses (strategic depth choice)
- Use fire assay for definitive answers on high-value targets

## Mode Switching UI

Per-stage implicit override: Player interacting with a stage automatically overrides auto for that stage. The AI continues to handle other stages unless the player intervenes there too.

The header shows a global toggle `[Default / Manual]` that sets the baseline, with per-stage overrides applied on top.

## Open Questions

| Question | Status |
|----------|--------|
| Should the AI improve over time (learns from player behavior)? | Deferred — not for initial implementation |
| Can the player customize AI priorities (e.g., "prefer deep sampling")? | Deferred — possible future feature |
| Does AI mode consume more or fewer resources than manual? | Same resource costs, just worse decisions |
| Exact confidence penalty percentages at each tier | Needs playtesting to finalize |
