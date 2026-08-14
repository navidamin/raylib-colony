# Excavation — Implementation Plan

> Status: DRAFT — plan only, no code written
> Last Updated: 2026-08-13
> Parent: [README.md](README.md) · Design: [excavation-design.md](excavation-design.md)

---

## 0. Branch Situation — Do This First

The prospecting work this design depends on lives on
**`claude/game-status-remaining-z2u35f`**, 23 commits ahead of this branch. Excavation
cannot be built against `main`'s prospecting — it's already stale.

### Merged as of 8b6e880 — clean, no conflicts

The merge is done. What follows is what it brought in.

### What that branch changes that matters here

| Change | Why it matters to excavation |
|--------|----------------------------|
| **Fixed 8×8 lattice; tier extends reach in rings** (2×2 → 8×8), replacing the old resizing grid | Rewrote the design's foundation. Reach is now the progression axis, and excavation reads it with its own tier |
| **Quantity / composition split** — `GetQuantity()` added alongside `GetGroundTruth()`, which now returns *fractions* | Excavation must consume both, deliberately (§2) |
| `RICHNESS_NORMALIZATION` 2.0 → 10000.0, calibrated against dumped data | Any excavation constant touching richness must be calibrated the same way |
| `rendermanager.cpp` +1603 lines — extraction UI rebuilt to the dark theme | The excavation panel must be built in the *new* renderer, not the old one |
| `src/CMakeLists.txt` restructured around `COLONY_CORE_SOURCES` | New excavation sources go in that list, once, for all three targets |
| `tools/preview`, `tools/playtest`, `tools/inspect`, `tools/shell-test` | The test loop this plan uses (§5) |
| `docs/guides/module-architecture.md`, `ui-panels.md`, `feature-completeness.md` | House rules this plan follows |

### Merge, don't rebase

This branch is documentation only so far, so a merge is clean and keeps both histories.

```bash
git merge origin/claude/game-status-remaining-z2u35f     # done: 8b6e880, auto-merged
```

Both expected conflicts (`CLAUDE.md`, `docs/design/prospecting/README.md`) resolved
automatically. **Re-merge before starting each phase** — that branch is still moving, and
its grid work is this module's foundation.

---

## 1. Architecture

Follows `docs/guides/module-architecture.md` Part II exactly. Prospecting is the reference
implementation; excavation mirrors its shape.

```
src/Excavation/
    excavation_constants.h      machine table, tier gates, tuning constants
    excavation_types.h/.cpp     Machine, DigTarget, DigResult, MachineState
    dig_engine.h/.cpp           pure logic: yield composition, power, wear per tick
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

| Tier / reach | Best vs mean, **targeted resource** | Best vs mean, **total quantity** |
|--------------|------------------------------------|----------------------------------|
| T0 · 2×2 | **+33%** | ~+16% |
| T3 · 8×8 | **+130%** | ~+65% |

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

### Phase 1 — Read-only adapter ✅ DONE
Prove excavation can see prospecting's grid before changing anything.

- `ExcavationSystem` facade, owned by `Unit`, created when the EXCAVATION module exists
- Read-only accessors over `prospectingSystem->GetGrid()`: per-spot quantity, composition,
  confidence, depth availability
- **Reach, using the excavation module's own tier** — `IsSubCellInReach(x, y, excavationTier)`
- `tools/inspect` dump of the grid as excavation sees it

**Done.** `src/Excavation/` has `excavation_constants.h`, `excavation_types.h`,
`site_view.{h,cpp}` (engine) and `excavation_system.{h,cpp}` (facade); `Unit` owns the
facade and keeps its tier in sync; `colony_inspect` dumps the lattice as excavation sees it.
`ProcessExtraction()` is untouched.

`SiteView` takes the **grid and tray**, not `ProspectingSystem` — an engine should depend on
data, not on another module's facade. That is also what lets `colony_inspect` drive it
without linking the whole prospecting chain.

**What the dump showed** (parent cell 5,5, target C — the cell's dominant resource):

| Tier | Reach | Spots | Mean | Best | Best vs mean | Reach holds |
|------|-------|-------|------|------|--------------|-------------|
| T0 | 2×2 | 4 | 551 | 854 | +55% | **38%** of the lattice's best |
| T1 | 4×4 | 16 | 834 | 1935 | +132% | 87% |
| T2 | 6×6 | 36 | 969 | 2219 | +129% | 100% |
| T3 | 8×8 | 64 | 968 | 2219 | +129% | 100% |

Close to the simulation (+33% → +130%), and it confirms the design's central story from real
data: **at tier 0 you can only reach 38% of the best ground on your own lattice.** The best
spot also moves between depth layers — SURFACE peaks at (6,4), MID at (1,1) — which is the
"depth re-rolls the clusters, you cannot extrapolate downward" claim, observed rather than
assumed.

### Phase 2 — Estimate engine ✅ DONE
The gamble, with nothing depending on it yet.

- `EstimateEngine::GetEstimate(spot, depth, target)` → `{ low, high, shown }`
- Blur from a **stable hash of (parentGridX, parentGridY, sx, sy, depth, resourceIdx)** —
  reuse prospecting's `HashSeed` pattern. **Never `rand()`, never per-tick.**
- `spread = maxSpread × (1 − confidence(spot, depth))`, confidence read per depth

**Done.** `estimate_engine.{h,cpp}` plus a `colony_test` target — the repo had no test
framework, so it is a plain executable with a `Check()` helper that exits non-zero on
failure. **31 checks, 0 failures.**

The estimate is built so the **range always contains the truth** — the instrument is
imprecise, never lying. What moves is where inside the range the point estimate sits:

```
halfWidth = truth × MAX_SPREAD × (1 − confidence)
shown     = truth + stableOffset × halfWidth      // |stableOffset| ≤ 1
low, high = shown ∓ halfWidth
```

`stableOffset` is an FNV-1a hash of *(parent cell, sub-cell, depth, resource)*, matching the
pattern `ProspectingGrid::HashSeed` already uses. Never `rand()`, never per-tick.

There is no save system yet, so the meaningful equivalent of a save/load test is rebuilding
the world from the same seed and re-reading all 64 spots — which the test does.

**What the dump showed** — the best reachable spot at cell (5,5) truly holds **2219**:

| Confidence | Player reads | Range |
|-----------|-------------|-------|
| 0% | **937** | 0 – 2268 |
| 25% | 1257 | 259 – 2256 |
| 50% | 1578 | 912 – 2243 |
| 75% | 1898 | 1566 – 2231 |
| 100% | **2219** | 2219 – 2219 |

Better than designed: **the best spot on the lattice reads as a poor one when unsurveyed.**
A blind player doesn't just get a fuzzy number — they walk straight past the best ground.
That is Rule 4's hindsight moment arriving on its own, with no nagging and no special-casing.

> Worth watching in balance: the bias is stable per spot, so a *particular* rich spot always
> under-reads. That is intended (Rule 1), but if too many rich spots under-read the early
> game may feel uniformly poor rather than uncertain. Check in Phase 7.

### Phase 3 — Dig engine ✅ DONE
The core, still with no new UI.

- `Machine` struct + machine table in `excavation_constants.h` (**depth** reach, precision,
  pace ceiling, power floor, selectivity, wear) — a constant table, not branching logic.
  Spatial reach is the tier ring and is *not* a machine stat
- `DigEngine::Tick()` → `{ perResourceYield, powerDraw, wearDelta }` — the yield map *is* the
  composition, so how dirty the dig was needs no separate field
- **Precision** implemented as a weighted blend of the aimed spot with its neighbours —
  this is the stat that ties machinery to the survey (design §4)
- Per-spot depletion, written back to `ResourceManager::UpdateResourceDepletion()`
- Existing multipliers preserved: `opsModifier`, `directiveModifier`, tier multiplier,
  machine count — Operations and Directives keep working untouched

**Done.** `dig_engine.{h,cpp}` plus the machine table, `DigSite` depletion state on the
facade, and `ProcessExtraction()` Stage 1 replaced. **55 checks, 0 failures.**

Excavation now works **one spot** on the lattice with a chosen machine rather than skimming
the whole parent cell evenly. Every previous modifier survives with its old meaning —
operations efficiency, directives, module tier and survey gating are folded into a single
`externalMultiplier` handed to the engine, so Operations and Directives keep working
untouched. Stages 2 and 3 are unchanged.

**The two mechanics that carry the design:**

*Precision* — a machine below 1.0 blends the aimed spot with its in-reach neighbours. That
is what makes a blunt machine throw away a survey, and what makes it the *right* tool on
unsurveyed ground, where covering ground beats aiming.

*Selectivity* — how much surrounding waste the machine leaves in the ground. It shows up in
the composition handed onward, never as a purity number. Pushing the pace costs selectivity,
so the pace dial trades tonnage against mix.

**Machine table** — six machines, none a strict upgrade on another. The pair that matters is
Bucket Wheel and Bucket Drum, both arriving at tier 1 as opposites: 1.8 pace / 0.45 precision
/ 0.10 selectivity against 1.0 / 0.90 / 0.60.

**Two bugs found by the tests, not by reading:**

1. *Zeno's excavator.* A nearly-empty spot yielded proportionally less, so depletion
   approached zero and a spot **never actually ran out** — 100,000 ticks and still going.
   Fixed with a floor on the taper (`EXC_MIN_TAPER`). A spot now exhausts after ~65 ticks at
   full pace, about three game days.
2. Depletion rate was 4× too slow to be observable; recalibrated against the real spot
   quantities `colony_inspect` reports.

**Also changed:** wear now follows work done rather than the clock, and the PRIORITIZE
directive steers what excavation *aims at* rather than applying a flat +40% to one resource
— more direct, and it uses the machinery that now exists.

A fallback keeps the old flat per-cell skim for units without an excavation or prospecting
system, so harnesses and any older path still produce.

### Phase 4 — Write-back to prospecting ✅ DONE
The one change that reaches into another module.

- `SubCell` gains a worked state: how much has been taken, per depth
- One setter on `ProspectingGrid` — **excavation calls prospecting, never the reverse**
- Digging sets that spot/depth confidence to 1.0 and flags it *known by digging*
- Prospecting's grid renderer gains the distinct mark (100% known **and** emptied)

**Done.** **67 checks, 0 failures.** Four small touches in `src/Prospecting/`, one in the
renderer:

| Change | File |
|--------|------|
| `SubCell::workedFraction[4]` + `HasBeenDug(d)` | `prospecting_types.h` |
| `RecordExcavation()` — the single setter — and `GetExcavatedKnowledge()` | `prospecting_grid.{h,cpp}` |
| A cell counts as known by whichever route got further: sweep **or** dug | `survey_progress_engine.cpp` |
| Amber corner wedge for dug spots, plus an `Excavated: n/4 layers` readout | `rendermanager.cpp` |

The dependency runs **one way**: excavation calls `RecordExcavation`, prospecting never
reaches into excavation.

**Confidence is per depth, and digging respects that.** `SiteView::GetConfidence` returns
1.0 for a dug layer only — digging the surface says nothing about what lies under it, which
is what keeps the deep layers a bet long after the surface is mapped.

**Digging bootstraps survey, and self-limits.** Because dug knowledge enters through the
sweep term, and `SURVEY_SWEEP_WEIGHT` is 0.20, a player who digs the entire lattice at every
depth and never surveys tops out at **20% survey progress** — about 0.48 extraction
efficiency against 0.35 for a blind start and 1.0 for a full survey. Exactly Rule 3's shape:
a legitimate road to knowledge that is distinctly the worse one. The cap falls out of
prospecting's existing weights rather than anything invented here.

**A gap the render caught.** With the mark drawn but nothing else changed, a fully dug spot
still read *"Confidence: Very Low"* — because that number measures what the *instruments*
found, and digging does not raise it. Correct internally, plainly wrong to a player looking
at ground they had already emptied. Fixed with the amber `Excavated: n/4 layers` line, which
is also what Rule 5 asked for: the mark has to carry *what was there and how much is gone*,
not just say "known".

A `worked` preview state renders dug-but-never-surveyed ground so this stays checkable.

### Phase 5 — The panel ✅ DONE
Built per `docs/guides/ui-panels.md`: procedural primitives, existing design tokens, IMGUI
discipline, touch-first targets.

- Grid widget — spots shaded by **targeted-resource yield** (§2), depth tabs, dug/surveyed/
  unknown states
- Machine bay — cards with AUTO default
- Target selector, pace slider, power cap slider
- The bottom line: **expected vs actual**, plus useful-output-per-power

**Done.** `DrawExcavationPanel` rewritten — the legacy fleet table is gone. Grid left,
controls right, the expected-vs-actual line across the bottom. **67 checks, 0 failures.**

The grid is shaded by **what the player has been told**, never by the truth — otherwise the
map would quietly hand over the survey they had not paid for. Brightness is target-resource
yield; opacity is confidence; worked-out ground drains back toward the base colour.

**Four things the render caught that the code did not:**

1. **A blank grid.** The default target was Fe and the test cell holds none, so every spot
   painted empty. Added `EnsureTargetPresent` — a target the ground does not hold gives the
   player nothing to choose between.
2. **Every machine locked.** `IsMachineAvailable` checked `UnlockRegistry` *and* tier, but
   the tech that unlocks a machine is the same tech that unlocks the module tier carrying it
   — so it gated the same thing twice, and made every machine unavailable in the harnesses,
   which reach high tiers via `DebugUpgradeModuleTier` precisely because that bypasses tech.
   Now tier only.
3. **AUTO showed a stale machine.** `SelectAutoMachine` only ran on a dig tick, so the panel
   displayed the constructor default until the unit had dug once. Added `SyncToGround`,
   called by both the panel and the tick.
4. **The AUTO chip sat on top of a machine card**, and the lock badge read `T2` — which
   looks like a rating rather than a requirement. Now `TIER 2`.

**Also:** the preview harness applied prospecting state only when previewing the prospecting
panel. Ground state is a property of the world, not of the panel being looked at, so
excavation could not be previewed against surveyed ground at all — which is the single most
important comparison for this panel. Fixed.

**What the screenshots show:**

| State | Reads as |
|-------|----------|
| Tier 0 | Only the central 2×2 reachable, everything else dashed and locked; Scoop alone, the rest marked `TIER 1/2/3` |
| Unsurveyed | `SPOT 4,4  H2  656  (116-1196, Very Low)` — AUTO picks **Bucket Wheel**, the blunt fast machine, because covering ground beats aiming when you cannot aim |
| Surveyed | `SPOT 4,4  H2  720  (323-1118, Low)` — the range tightens and the estimate moves toward the truth |

A legend under the grid names what the shading means, because brightness and opacity carry
two different meanings at once and neither is self-evident.

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

## 7. Decisions (2026-08-13)

| Question | Decision |
|----------|----------|
| ~~**Purity**~~ | **Reversed 2026-08-13.** There is no purity value in excavation. Beneficiation's separation chain already *is* the purity system, and "dug dirty" is already expressible in the `map<ResourceType, float>` Stage 1 hands over — more of everything else relative to the target. A separate scalar said the same thing twice and invented a cross-module hook to carry it. **Pace changes the composition, not a purity number**, and dirty digging costs through systems that already exist: capacity-limited sect storage with an overflow buffer, and a separation chain with more to process |
| **Machines** | **All stack on one spot** — more machines means faster, same place. One selection, one readout, small UI |
| **Excavation reach** | **Its own tier**, via `IsSubCellInReach(x, y, excavationTier)`. Implemented in Phase 1 |
| **Power cap** | Draws on the **sect's shared energy pool** — energy is already a sect resource |
| **Result reveal** | **Gradually as you dig**, not in a lump at the end |
| **Constants** | Calibrated against dumped data in Phase 7, not guessed now |

Still needs a word with whoever owns the prospecting branch: prospecting refuses
out-of-reach sweeps and drills, so both modules must agree that **reach is per-module**.
Nothing built so far depends on their answer — excavation only *reads*.

---

## 8. Still Open Before Phase 3

- `[?]` What is a spot's total yield before it exhausts? Sets how often the player moves, and needs calibrating against the real quantities (`colony_inspect` shows 760–44,000 per spot depending on the cell)

The first three are needed **before Phase 3**. The fourth can wait for Phase 5.
