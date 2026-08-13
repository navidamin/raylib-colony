# Excavation Module — Design Documents

> **Auto-context rule:** When working on excavation-related code (`src/Unit/unit.cpp` `ProcessExtraction()` Stage 1 and excavator handling, `src/Engine/rendermanager.cpp` excavation panel, excavation input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [excavation-design.md](excavation-design.md) | **The design.** Panel + place + gamble + machinery, with the rules that keep the gamble honest | DRAFT |
| 2 | [design-options.md](design-options.md) | Round 1 exploration: 16 aspects, then 4 options compared | SUPERSEDED |
| 3 | [design-options-v2.md](design-options-v2.md) | Round 2 exploration: three variants of the panel direction | SUPERSEDED |
| 4 | [excavation-mechanics.md](excavation-mechanics.md) | Science review of real excavation technologies (Part 1 is the reference; Part 3's A/B/C alternatives are superseded) | REFERENCE |

## Design Summary

Excavation is the second stage of the extraction pipeline. Prospecting tells you what's in
the ground; excavation gets it out; beneficiation separates it.

```
[Prospecting]  →  [EXCAVATION]  →  [Beneficiation]  →  [Sect Storage]
 what's there      get it out       separate it
```

**The design:** you point a machine at a spot in the ground and tune how hard it works.
Prospecting tells you which spot is worth it — if you paid for prospecting. Full detail in
[excavation-design.md](excavation-design.md).

| Pillar | What it is |
|--------|-----------|
| **The panel** | Target material, pace, power cap. Purity is what you read, not what you set |
| **Place** | Which spot in prospecting's grid, and how deep. No reach, no names |
| **The gamble** | Digging without having paid to survey first — not random noise, a choice to skip a cost |
| **Machinery** | Six machines with different reach, precision, pace and wear. Optional; AUTO by default |

The three lock together through one machine stat: **precision**. A sloppy machine digs
wider than you aimed and averages your chosen spot with its neighbours — throwing away the
survey you paid for. So *"should I survey?"* and *"which machine?"* become the same question.

Excavation invents no geography. Prospecting already builds a per-unit sub-cell grid (3×3 up
to 6×6 by tier), with real 0.3×–2.0× variation between spots, per-spot confidence, and
per-spot per-depth ground truth. Excavation reads that grid and digs in it.

The background fact behind the machine stats: **on the Moon a digger cannot push.** At 1/6
gravity a machine barely weighs anything, so it can't lean on a blade the way an Earth
excavator does. Real designs work around this by taking many small bites, by shaking the
soil loose first, or by blowing it out with gas — which is why different machine types have
genuinely different strengths rather than just bigger numbers. Details in
[excavation-mechanics.md](excavation-mechanics.md) Part 1.

## Cross-References

### Source Code (current implementation)

| File | Relevant Code |
|------|--------------|
| `src/Unit/unit.h` | `Excavator` struct (105-113), excavator getters/setters (120-124) |
| `src/Unit/unit.cpp` | Excavator init (439-447), EXCAVATION module def (467-490), tier descriptions (831-840), tier excavator spawning (878-900), energy cost (1108-1118), `ProcessExtraction()` Stage 1 (1299-1331), wear accumulation (141-143) |
| `src/Engine/rendermanager.cpp` | Excavation panel rendering |
| `src/Engine/inputmanager.cpp` | Excavation input handling |
| `src/ResourceManager/` | `GetResourcesAtGridLayer()` — per-cell, per-depth ground truth |
| `src/Prospecting/prospecting_grid.h` | **The grid excavation digs in** — `GetSubCell()`, `GetGroundTruth(subX, subY, depth)` |
| `src/Prospecting/prospecting_types.h` | `SubCell::aggregateConfidence` — how well a spot is known |
| `src/Prospecting/prospecting_constants.h` | `PROSPECTING_GRID_SIZE` (3–6), `SUBCELL_VARIATION_MIN/MAX` (0.3–2.0), `MAX_DEPTH_PER_TIER` |
| `src/game_constants.h` | `EXTRACTION_PRODUCTION_COSTS`, survey constants |

### Current State (what already exists)

The excavation module is implemented as a **data model only** (ROADMAP Phase E). What
exists today:

- `Excavator { id, gridPos, method, depth, rate, wear }` — a flat struct, one per machine
- Tier gates excavator count (T0=1, T1=2, T2=4, T3=8) and `method` string
  (`scoop` → `bucket_wheel` → `percussive` → `drone`)
- Depth maps to a `DepthLayer` (SURFACE / SHALLOW / MID / DEEP) via the *first* excavator only
- Output = `baseRate × efficiency × tierMult × abundance × opsMod × directiveMod × scanMult × activeExcavatorCount`
- Wear accrues linearly; `wear >= 1.0` removes the excavator from the active count

Everything in this design directory is about replacing that multiply-chain with mechanics.

### Related Module Designs

| Module | Dependency | Status |
|--------|-----------|--------|
| **Prospecting** | Owns the grid excavation digs in, and the confidence that decides whether a dig is a bet or a calculation. The tightest coupling in the game | [`docs/design/prospecting/`](../prospecting/README.md) — DRAFT |
| **Beneficiation** | Direct downstream consumer — receives whatever excavation digs up | Not yet in design/ |
| **Operations** | Supplies the efficiency modifier applied to excavation output | Not yet in design/ |
| **Directives** | `MAXIMIZE` / `CONSERVE` / `EMERGENCY_HARVEST` all act on excavation rate and wear | Not yet in design/ |
| **Energy** | Excavation is the unit's largest power draw; the power-cap slider makes that a direct player concern | Not yet in design/ |
| **AI Automation** | Second client of the shared pattern. Every player input is a value, so the AI uses the same interface; research improves how well it chooses, including when to gamble | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

## Implementation Order

To be written. The design's open questions in
[excavation-design.md §9](excavation-design.md#9-open-questions) come first — three of them
need checking against the prospecting code before build order is meaningful.

## Key Design Constraint

Whatever excavation ends up being, it has to feed the existing pipeline without
restructuring it:

```
Stage 1 (excavation)   → what gets dug up      ← this is what we're designing
Stage 2 (beneficiation separation chain) — unchanged
Stage 3 (sect storage) — unchanged
```

The existing modifiers (Operations efficiency, Directives, module tier, machine count) all
still multiply into Stage 1 and keep their current meaning, so those modules keep working.
