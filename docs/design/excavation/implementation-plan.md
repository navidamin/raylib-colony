# Excavation — Implementation Plan

> Status: DRAFT — plan only, no code written
> Last Updated: 2026-08-13
> Parent: [README.md](README.md) · Design: [excavation-design.md](excavation-design.md)

---

## 0. Branch Situation — Do This First

The prospecting work this design depends on lives on
**`claude/game-status-remaining-z2u35f`**, 23 commits ahead of this branch. Excavation
cannot be built against `main`'s prospecting — it's already stale.

### What that branch changes that matters here

| Change | Why it matters to excavation |
|--------|----------------------------|
| **Quantity / composition split** — `GetQuantity()` added alongside `GetGroundTruth()`, which now returns *fractions* | This is the single most important change. Excavation must consume both, deliberately (§2) |
| `RICHNESS_NORMALIZATION` 2.0 → 10000.0, calibrated against dumped data | Any excavation constant touching richness must be calibrated the same way |
| `rendermanager.cpp` +1603 lines — extraction UI rebuilt to the dark theme | The excavation panel must be built in the *new* renderer, not the old one |
| `src/CMakeLists.txt` restructured around `COLONY_CORE_SOURCES` | New excavation sources go in that list, once, for all three targets |
| `tools/preview`, `tools/playtest`, `tools/inspect`, `tools/shell-test` | The test loop this plan uses (§5) |
| `docs/guides/module-architecture.md`, `ui-panels.md`, `feature-completeness.md` | House rules this plan follows |

### Merge, don't rebase

This branch is documentation only so far, so a merge is clean and keeps both histories.

```bash
git fetch origin claude/game-status-remaining-z2u35f
git merge origin/claude/game-status-remaining-z2u35f
```

**Two files conflict, both documentation, both trivial:**

| File | Conflict | Resolution |
|------|----------|-----------|
| `CLAUDE.md` | Both added rows to the design-docs auto-context table | Keep both rows |
| `docs/design/prospecting/README.md` | They rewrote status/TOC; we edited the excavation row in the related-modules table | Take theirs, re-apply our one-line excavation row |

> If `game-status-remaining` is meant to land on `main` first, wait for that and merge
> `main` instead. Either way, **do not start coding until the merge is done** — the
> quantity/composition split changes what excavation reads on day one.

---

## 1. Architecture

Follows `docs/guides/module-architecture.md` Part II exactly. Prospecting is the reference
implementation; excavation mirrors its shape.

```
src/Excavation/
    excavation_constants.h      machine table, tier gates, tuning constants
    excavation_types.h/.cpp     Machine, DigTarget, DigResult, MachineState
    dig_engine.h/.cpp           pure logic: yield, purity, power, wear per tick
    estimate_engine.h/.cpp      pure logic: the blur (Rules 1-2), expected-vs-actual
    machine_engine.h/.cpp       pure logic: machine stats, AUTO selection, repair
    excavation_system.h/.cpp    facade: owns engines + panel UI state
```

- **Engines are pure logic.** Data in, results out. No rendering, no input, no globals.
- **The facade owns UI state** — selected spot, selected depth, active machine, pace, power
  cap, last result — because the renderer is immediate-mode and keeps nothing between frames.
- **`Unit` owns the facade** as a `unique_ptr`, created when the EXCAVATION module exists,
  exactly as it owns `ProspectingSystem` today.
- All new sources go into **`COLONY_CORE_SOURCES`** in `src/CMakeLists.txt` — once, and all
  three targets pick them up.

---

## 2. The Units Trap — Read This Before Writing `dig_engine`

`docs/guides/module-architecture.md` Part II §2 calls the quantity/fraction mix-up *"the
most expensive bug in this codebase so far."* Excavation is the next place it will happen,
because it consumes **both** values and multiplies them together.

```cpp
// what the spot is made of — fractions, 0-1, sum to ~1
std::map<ResourceType, float> composition = grid.GetGroundTruth(sx, sy, depth);

// how much is there — absolute, thousands
float quantity = grid.GetQuantity(sx, sy, depth);

// yield of the targeted resource
float targetYield = quantity * composition[target];
```

### Why this product is exactly the right thing to optimise

The generator applies an independent cluster field to each resource, then normalises. So the
product is algebraically a lossless round-trip back to the targeted resource's own field:

```
quantity × composition[t] = Σᵢ(abundanceᵢ × wᵢ) × (abundance_t × w_t) / Σᵢ(abundanceᵢ × wᵢ)
                          = abundance_t × w_t
```

Which means the survey-value numbers measured in
[excavation-design.md §3](excavation-design.md#why-anyone-would-dig-blind) apply **to the
targeted resource**, not to total tonnage. Measured separately:

| Grid | Best vs mean, **targeted resource** | Best vs mean, **total quantity** |
|------|------------------------------------|----------------------------------|
| 3×3 | **+50%** | +24% |
| 6×6 | **+114%** | +57% |

Total quantity is much flatter because independent per-resource clusters partly cancel when
summed.

> **UI rule that falls out of this:** the grid must shade by **targeted-resource yield**,
> never by total quantity. Shading by total would show a map half as varied as the one the
> player is actually choosing on — and it must **re-shade when the target changes**, which
> is what makes the target selector visibly drive the map.

Reproduce with `docs/design/excavation/subcell_distribution_sim.py`.

---

## 3. Build Order

Seven phases. Each is independently verifiable; the game stays playable throughout.

### Phase 1 — Read-only adapter *(no behaviour change)*
Prove excavation can see prospecting's grid before changing anything.

- `ExcavationSystem` facade, owned by `Unit`, created when the EXCAVATION module exists
- Read-only accessors over `prospectingSystem->GetGrid()`: grid size, per-spot quantity,
  composition, confidence, depth availability
- `tools/inspect` dump of the grid as excavation sees it

**Done when:** the inspect dump shows plausible per-spot yields for a chosen target, and
`ProcessExtraction()` still behaves exactly as before.

### Phase 2 — Estimate engine *(Rules 1 and 2)*
The gamble, with nothing depending on it yet.

- `EstimateEngine::GetEstimate(spot, depth, target)` → `{ low, high, shown }`
- Blur from a **stable hash of (parentGridX, parentGridY, sx, sy, depth, resourceIdx)** —
  reuse prospecting's `HashSeed` pattern. **Never `rand()`, never per-tick.**
- `spread = maxSpread × (1 − confidence(spot, depth))`, confidence read per depth

**Done when:** a unit test asserts the same spot returns an identical estimate across
1,000 calls and across a save/load, and that spread → 0 as confidence → 1.

### Phase 3 — Dig engine, replacing the Stage 1 multiply chain
The core, still with no new UI.

- `Machine` struct + machine table in `excavation_constants.h` (reach, precision, pace
  ceiling, power floor, purity, wear) — a constant table, not branching logic
- `DigEngine::Tick()` → `{ perResourceYield, purity, powerDraw, wearDelta }`
- **Precision** implemented as a weighted blend of the aimed spot with its neighbours —
  this is the stat that ties machinery to the survey (design §4)
- Per-spot depletion, written back to `ResourceManager::UpdateResourceDepletion()`
- Existing multipliers preserved: `opsModifier`, `directiveModifier`, tier multiplier,
  machine count — Operations and Directives keep working untouched

**Done when:** output is within ~10% of current values on an average spot, and a precise
machine on a surveyed spot measurably beats a sloppy one.

### Phase 4 — Write-back to prospecting *(Rule 5)*
The one change that reaches into another module.

- `SubCell` gains a worked state: how much has been taken, per depth
- One setter on `ProspectingGrid` — **excavation calls prospecting, never the reverse**
- Digging sets that spot/depth confidence to 1.0 and flags it *known by digging*
- Prospecting's grid renderer gains the distinct mark (100% known **and** emptied)

**Done when:** digging a blind spot visibly changes its state in the prospecting panel, and
`surveyProgress` rises from digging alone.

### Phase 5 — The panel
Built per `docs/guides/ui-panels.md`: procedural primitives, existing design tokens, IMGUI
discipline, touch-first targets.

- Grid widget — spots shaded by **targeted-resource yield** (§2), depth tabs, dug/surveyed/
  unknown states
- Machine bay — cards with AUTO default
- Target selector, pace slider, power cap slider
- The bottom line: **expected vs actual**, plus useful-output-per-power

**Done when:** `tools/preview` screenshots look right at every tier, and the panel is usable
on the phone build.

### Phase 6 — AUTO and the AI ladder
- `MachineEngine::SelectAuto()` — sensible machine for the spot
- AI levels from design §7: *Basic* (best known spot, never gambles) → *Trained* (weighs
  unknowns) → *Expert* (also schedules surveys)
- Efficiency penalty vs player control, consistent with prospecting's default mode

**Done when:** a unit left entirely alone plays a competent game, roughly 15% behind a
tuned one.

### Phase 7 — Balance
- Calibrate every constant against **dumped real data**, per the guide — no invented numbers
- Verify the survey-value arc actually lands at +50% (T0) → +114% (T3) in play
- Check the `SUBCELL_VARIATION_MAX` ceiling: at 6×6 the best spot is pinned to the 2.0 clamp
  82% of the time, so raise the clamp rather than the grid if T3 needs headroom

---

## 4. Sequencing Notes

```
Phase 1 ─▶ 2 ─▶ 3 ─▶ 4 ─▶ 6 ─▶ 7
                 └──▶ 5 (can run parallel to 4 and 6)
```

- **Phases 1–3 change no other module.** If the plan stalls, everything up to here is still
  a net improvement over the current multiply chain.
- **Phase 4 is the only cross-module change.** It touches `src/Prospecting/`, so it should
  be a separate commit, reviewed on its own, and coordinated if the prospecting branch is
  still active.
- **Phase 5 can start once Phase 3 produces real numbers** — it needs data to render, not
  the write-back.

---

## 5. Testing

Using the instruments the prospecting branch built (`docs/dev-workflow.md`):

| Instrument | Use for |
|-----------|---------|
| `tools/inspect` | Phase 1 and 7 — dump the grid as excavation sees it; calibrate constants against real numbers |
| `tools/preview` | Phase 5 — headless screenshots of the panel at each tier |
| `tools/playtest` | Phases 3, 6, 7 — drive a unit to any tier via `DebugUpgradeModuleTier()`, no economy needed |
| `tools/shell-test` | Phase 5 — web/phone canvas regression |
| `subcell_distribution_sim.py` | Phase 7 — re-derive survey value if the generator ever changes |

Two things worth asserting in code rather than by eye:
- **Rule 1** — estimates are stable across calls and across save/load
- **The units trap** — that `quantity × composition` is used for yield, and never quantity
  alone or fractions alone

---

## 6. Risks

| Risk | Mitigation |
|------|-----------|
| **Quantity/fraction mix-up** — the guide's named worst bug, and excavation multiplies both | Name units in every signature; assert in Phase 3; calibrate against dumped data |
| **Phase 4 collides with active prospecting work** | Keep it a separate commit; a single setter is a small, reviewable surface; coordinate before starting |
| **The blur becomes a slot machine** | Rule 1 has a unit test, not just a note in the doc |
| **The panel gets crowded** — grid + machines + 2 sliders + target + readout | Machinery is optional and AUTO by default; consider a collapsed machine bay at low tier |
| **The renderer moved 1,600 lines** | Merge first (§0), then build the panel against the new code |

---

## 7. Still Open Before Phase 3

From [excavation-design.md §9](excavation-design.md#9-open-questions), these change the code
rather than the wording:

- `[?]` Does purity change **what** beneficiation receives, or **how much power** it burns to separate it?
- `[?]` Does the power cap draw on the sect's shared pool, or is it a local budget?
- `[?]` Can several machines work different spots at once, or do they stack on one spot? Stacking is simpler; splitting needs more UI and more facade state.
- `[?]` Is a disappointing result revealed gradually as you dig, or all at once at the end?

The first three are needed **before Phase 3**. The fourth can wait for Phase 5.
