# Excavation Module — Design Documents

> **Auto-context rule:** When working on excavation-related code (`src/Unit/unit.cpp` `ProcessExtraction()` Stage 1 and excavator handling, `src/Engine/rendermanager.cpp` excavation panel, excavation input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [design-options-v2.md](design-options-v2.md) | **Current.** Three variants of the chosen direction — a control panel plus optional machinery, a gamble, and a sense of place | DRAFT — awaiting decision |
| 2 | [design-options.md](design-options.md) | Round 1: the 16 aspects any excavation design must answer, then 4 options compared. Option 3 was preferred and is narrowed in the doc above | SUPERSEDED — Option 3 chosen |
| 3 | [excavation-mechanics.md](excavation-mechanics.md) | Science review of real excavation technologies (Part 1 is the reference; Part 3's A/B/C alternatives are superseded) | REFERENCE |

## Design Summary

Excavation is the second stage of the extraction pipeline. Prospecting tells you what's in
the ground; excavation gets it out; beneficiation separates it.

```
[Prospecting]  →  [EXCAVATION]  →  [Beneficiation]  →  [Sect Storage]
 what's there      get it out       separate it
```

**Direction chosen: a control panel** — the player sets a target material and two sliders
(pace, power cap) and reads the result, rather than driving machines directly. It's the
friendliest of the four options considered, and because every player action is *setting a
value*, an AI can use the identical interface — so automation needs no parallel system.

Three variants add machinery, uncertainty, and a sense of place on top of that core.
Details in [design-options-v2.md](design-options-v2.md):

| | Place is… | Gamble is… | Machinery is… | Effort |
|---|---|---|---|---|
| **A — The Field Picker** | a short list of named grounds | a range on the estimate | a picker, AUTO by default | Low |
| **B — The Working Map** | a small map that visibly wears down | fog over unworked ground | tied to what the ground needs | Medium |
| **C — The Standing Order** | how far your operation reaches | the price of reaching further | a rule in your order | Low–Med |

These stack rather than compete: build **A**, use **C**'s order sheet as the tier-3 AI mode,
and **B**'s map can replace A's place list later as a pure visual upgrade with no mechanical
change.

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
| **Prospecting** | Supplies survey progress and marked sites; how heavily excavation leans on it varies by variant (most tangible in Variant B, where surveying clears fog off the map) | [`docs/design/prospecting/`](../prospecting/README.md) — DRAFT |
| **Beneficiation** | Direct downstream consumer — receives whatever excavation digs up | Not yet in design/ |
| **Operations** | Supplies the efficiency modifier applied to excavation output | Not yet in design/ |
| **Directives** | `MAXIMIZE` / `CONSERVE` / `EMERGENCY_HARVEST` all act on excavation rate and wear | Not yet in design/ |
| **Energy** | Excavation is the unit's largest power draw; the power-cap slider makes that a direct player concern | Not yet in design/ |
| **AI Automation** | Excavation is the second client of the shared pattern — and the control-panel direction means the AI uses the player's own interface rather than a parallel system | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

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
