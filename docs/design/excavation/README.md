# Excavation Module — Design Documents

> **Auto-context rule:** When working on excavation-related code (`src/Unit/unit.cpp` `ProcessExtraction()` Stage 1 and excavator handling, `src/Engine/rendermanager.cpp` excavation panel, excavation input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [excavation-mechanics.md](excavation-mechanics.md) | Science review of excavation technologies, candidate functions, and 3 alternative module designs (A/B/C) | REVIEWED — alternatives decided |
| 2 | [dig-plan-design.md](dig-plan-design.md) | **The chosen design.** Design B + machinery selection, split into Level 1 (build now) and Level 2 (deferred) | DRAFT |

## Design Summary

Excavation is the second stage of the extraction pipeline. It takes the confidence map
produced by prospecting and converts ground into a *mass flow with a grade vector*, which
beneficiation then separates.

```
[Prospecting]         [EXCAVATION]                      [Beneficiation]
 confidence map  →  [Plan] → [Dig Cycle] → [Haul]  →  raw regolith + grade
 marked sites        where     how               how much     ↓ separation chain
                                                          [Sect Storage]
```

The central scientific fact the module is built around: **on the Moon an excavator cannot
push.** At 1/6 g, machine weight provides almost no reaction force, so terrestrial
"bigger blade, more down-force" logic does not apply. Every real design solves this by
taking *small bites continuously* (bucket drum / bucket wheel), by *reducing the soil's
strength* (percussion, ultrasonics, thermal), or by *avoiding contact forces entirely*
(pneumatic). That constraint is what makes excavation an interesting decision space
rather than a throughput slider.

Three alternative designs were proposed in [excavation-mechanics.md](excavation-mechanics.md):

| Design | Scale | Core Question | Core Tension | Outcome |
|--------|-------|--------------|--------------|---------|
| **A — Dig Cycle** | Machine | *How do I cut?* | Force vs traction vs wear | Reduced to machinery *selection* in L1; full tuning deferred to L2 |
| **B — Mine Plan** | Pit / site | *Where do I cut?* | Grade vs dilution vs haul distance | ✅ **CHOSEN — the spine** |
| **C — Fleet & Uptime** | Operation | *Can I keep cutting?* | Throughput vs wear vs power/thermal duty cycle | Deferred to L2 |

**Decision:** Design B with minor machinery selection, built in two levels.
See [dig-plan-design.md](dig-plan-design.md).

| Level | Scope | When |
|-------|-------|------|
| **Level 1** | Block model, machine selection, dig plan, dilution, depletion, auto mode. Self-contained and playable; no file outside excavation changes behavior. | **Now** |
| **Level 2** | Depth physics, machine tuning, wear/maintenance, duty cycle, bench geometry, haulage, fleet roles. | After surrounding modules are plugged in |

The Level 1 loop: queue blocks → see `PLANNED 0.42 → DELIVERED 0.27 (−36% DILUTION)` →
survey those blocks → the same plan now delivers far more. Machine choice feeds the same
decision, because coarse machines cost nothing in uniform ground and ruin isolated rich
pockets.

## Cross-References

### Source Code (current implementation)

| File | Relevant Code |
|------|--------------|
| `src/Unit/unit.h` | `Excavator` struct (105-113), excavator getters/setters (120-124) |
| `src/Unit/unit.cpp` | Excavator init (439-447), EXCAVATION module def (467-490), tier descriptions (831-840), tier excavator spawning (878-900), energy cost (1108-1118), `ProcessExtraction()` Stage 1 (1299-1331), wear accumulation (141-143) |
| `src/Engine/rendermanager.cpp` | Excavation panel rendering |
| `src/Engine/inputmanager.cpp` | Excavation input handling |
| `src/ResourceManager/` | `GetResourcesAtGridLayer()` — depth-layer ground truth |
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
| **Prospecting** | Supplies `surveyProgress` + `markedSites`; under Design B also supplies per-sub-cell confidence used for grade control | [`docs/design/prospecting/`](../prospecting/README.md) — DRAFT |
| **Beneficiation** | Direct downstream consumer — receives excavation's raw mass + grade vector | Not yet in design/ |
| **Operations** | Supplies the efficiency modifier applied to excavation output | Not yet in design/ |
| **Directives** | `MAXIMIZE` / `CONSERVE` / `EMERGENCY_HARVEST` all act on excavation rate and wear | Not yet in design/ |
| **Energy** | Excavation is the largest energy consumer in the unit; Design C makes the day/night duty cycle explicit | Not yet in design/ |
| **AI Automation** | Excavation is the second client of the shared AI/default-mode pattern | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

## Implementation Order

Full build order lives in [dig-plan-design.md §4](dig-plan-design.md#4-build-order-for-level-1).
Summary of Level 1:

1. `DigBlock` struct, 5×5×4 grid, populated from ResourceManager
2. `GetBlockConfidence()` adapter — the single seam to prospecting
3. `MachineSpec` loading from `game_types.toml`, tier/tech gating
4. `ComputeDilution()` + `ComputeBlockThroughput()`
5. `ProcessExtraction()` Stage 1 rewrite, per-block depletion
6. Survey constant retune + balance pass
7. `AutoSelectDigPlan()` — game plays correctly with the panel never opened
8. Panel UI

Steps 1–7 are gameplay-complete without step 8, so the UI can be iterated separately.

## Key Design Constraint

Whatever the excavation redesign produces, it must feed the existing pipeline unchanged:

```
Stage 1 (excavation) → std::map<ResourceType, float> rawRegolith
Stage 2 (beneficiation separation chain) — unchanged
Stage 3 (sect storage) — unchanged
```

The survey-gating formula in `ProcessExtraction()` keeps its shape, but Level 1 retunes two
constants so that `scanMultiplier` and the new dilution term don't penalize unsurveyed
ground twice:
```
scanMultiplier = (0.70 + 0.30 * surveyProgress) + (0.15 if marked)
                  ^^^^   ^^^^ was 0.35 / 0.65
```
The knowledge penalty moves from *rate* to *grade*. See
[dig-plan-design.md §2.6](dig-plan-design.md#26-throughput-and-integration-with-existing-multipliers).

New mechanics multiply into Stage 1; they must not restructure Stages 2–3.
