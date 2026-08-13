# Excavation Module — Design Documents

> **Auto-context rule:** When working on excavation-related code (`src/Unit/unit.cpp` `ProcessExtraction()` Stage 1 and excavator handling, `src/Engine/rendermanager.cpp` excavation panel, excavation input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [design-options.md](design-options.md) | **Current.** The 16 aspects any excavation design must answer, then 4 options defined side by side for comparison | DRAFT — awaiting decision |
| 2 | [excavation-mechanics.md](excavation-mechanics.md) | Science review of real excavation technologies (Part 1 is the reference; Part 3's A/B/C alternatives are superseded) | REFERENCE |

## Design Summary

Excavation is the second stage of the extraction pipeline. Prospecting tells you what's in
the ground; excavation gets it out; beneficiation separates it.

```
[Prospecting]  →  [EXCAVATION]  →  [Beneficiation]  →  [Sect Storage]
 what's there      get it out       separate it
```

**No design chosen yet.** Four options are laid out side by side in
[design-options.md](design-options.md), each answering the same 16 questions so they can be
compared directly:

| | Built on | Feels like | Main tension |
|---|---|---|---|
| **1 — The Pit** | The ground | Carving territory | Reach vs distance |
| **2 — The Machine Shed** | The equipment | Running a garage | Speed vs breakdown |
| **3 — The Dig Order** | The flow | Tuning an engine | Speed vs clean vs power |
| **4 — The Gamble** | The unknown | Placing bets | Knowing vs digging |

The background fact that shapes all of them: **on the Moon a digger cannot push.** At 1/6
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
| **Prospecting** | Supplies survey progress and marked sites; how heavily excavation leans on it varies by option (heaviest in Option 4) | [`docs/design/prospecting/`](../prospecting/README.md) — DRAFT |
| **Beneficiation** | Direct downstream consumer — receives whatever excavation digs up | Not yet in design/ |
| **Operations** | Supplies the efficiency modifier applied to excavation output | Not yet in design/ |
| **Directives** | `MAXIMIZE` / `CONSERVE` / `EMERGENCY_HARVEST` all act on excavation rate and wear | Not yet in design/ |
| **Energy** | Excavation is the unit's largest power draw; Option 3 makes that a direct player concern | Not yet in design/ |
| **AI Automation** | Excavation is the second client of the shared AI/default-mode pattern | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

## Implementation Order

To be written once an option is chosen.

## Key Design Constraint

Whatever excavation ends up being, it has to feed the existing pipeline without
restructuring it:

```
Stage 1 (excavation)   → what gets dug up      ← this is what we're designing
Stage 2 (beneficiation separation chain) — unchanged
Stage 3 (sect storage) — unchanged
```

The existing modifiers (Operations efficiency, Directives, module tier, machine count) all
still multiply into Stage 1 and keep their current meaning, so those modules keep working
whichever option is picked.
