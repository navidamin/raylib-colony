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

Nine phases. Each is independently verifiable; the game stays playable throughout.
Phases 1-6 are done; 7 is in progress; 8 is small; **9 is deliberately loose — see §9.**

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

### Phase 6 — AUTO and the AI ladder ✅ DONE
- `MachineEngine::SelectAuto()` — sensible machine for the spot
- AI levels from design §7: *Basic* (best known spot, never gambles) → *Trained* (weighs
  unknowns) → *Expert* (also schedules surveys)
- Efficiency penalty vs player control, consistent with prospecting's default mode

**Done.** `auto_pilot.{h,cpp}` — a pure engine that returns an `AutoDecision` without
applying it, so the panel can show what the automation *would* do and the tests can check
its judgement without digging. **86 checks, 0 failures.**

Excavation is the **first AI implementation in the game** — prospecting's
`AI_CONFIDENCE_PENALTY` constants exist but nothing reads them — so this sets the pattern
for `docs/design/ai-automation/`.

The whole ladder turns on **how a level treats what it does not know**:

| Level | Scores a spot by | Efficiency |
|-------|------------------|-----------|
| MANUAL | — the player chooses | 1.00 |
| BASIC | the **worst case** (`low`) — wide uncertainty counts against a spot | 0.85 |
| TRAINED | the **middle plus upside** (`shown + 0.55 × halfWidth`) — uncertainty attracts | 0.92 |
| EXPERT | as TRAINED, and names where surveying would pay for itself | 0.97 |

That is pessimism versus optimism-under-uncertainty, and it falls straight out of the
estimate engine's range — no separate risk model was needed. Automation always costs
something; attention is never free.

The AI drives spot, depth, machine and pace. **The power cap stays the player's** — it is a
standing constraint on the operation rather than part of running it.

**A claim the tests killed.** The doc said BASIC "never gambles — an unsurveyed spot has a
floor of zero". That is false: `low = shown − halfWidth` and `shown` carries a stable bias,
so a spot whose bias reads high has a *positive* floor even unsurveyed. BASIC is
**cautious, not abstaining** — it optimises the worst case. The description in the UI was
corrected to match.

**A test that was asserting an accident.** The first version checked that BASIC and TRAINED
pick *different* spots. They rightly agree when the richest spot also reads confidently, so
that was testing this map rather than the rule. Replaced with criterion-optimality: BASIC
must land on the best available floor, TRAINED on the best available upside.

### Phase 7 — Balance *(started: `colony_sim` built, three defects fixed)*
- Calibrate every constant against **dumped real data**, per the guide — no invented numbers
- Verify the survey-value arc actually lands at +50% (T0) → +114% (T3) in play
- Check the `SUBCELL_VARIATION_MAX` ceiling: at 6×6 the best spot is pinned to the 2.0 clamp
  82% of the time, so raise the clamp rather than the grid if T3 needs headroom

**Two measurements from `colony_measure_clusters`, worth acting on here:**

| Finding | Why it matters |
|---------|---------------|
| `SUBCELL_VARIATION_MAX` binds in **98.9%** of fields | The ceiling is not an outlier guard, it is the shape of essentially every ore body. Raising it is the only lever on peak richness — the grid cannot supply more |
| `SUBCELL_VARIATION_MIN` binds in **25.6%** of fields | It is doing real work in a quarter of fields, but the `maxInfluence = 0.1f` floor in `GenerateLayerDistribution` keeps the other three quarters above it. Barren ground bottoms out at w ≈ 0.41 on average, not 0.3 — so raising MIN would change less than it appears |

A consequence worth knowing before tuning either: normalisation sets the pre-clamp mean of
`w` to 1.0, then clamping the peak *down* pulls the post-clamp mean to **0.875**. So the
best sub-cell reads **2.30× the mean**, not 2.0×, and any balance target expressed against
"the mean" has to use the post-clamp figure.

### Phase 8 — Classes on the grid *(small, and gated on prospecting)*
Cosmetic to build, and the payoff is disproportionate: it puts a visual on the gamble, which
is the module's central pillar and currently has no key.

- Needs `GetResourceClass()` from
  [prospecting's classification plan](../prospecting/implementation-plan.md) Phase C1 —
  **that is the only dependency**, and it is a few lines
- Grid spots carry their class colour alongside the existing targeted-yield shading. Two
  channels, two questions: *how good* (shade) and *how sure* (class)
- The bottom readout names the class — `B2 · Iron 40–70% · INFERRED` — so *"I knew this was
  a bet"* is legible after the fact, which is what Rule 4 asks for
- The resource icon's three-segment ring needs prospecting Phase C4's roll-up

**Verify with `tools/preview`** at every tier and every class, and look at the PNGs.

> **Watch the two-channel load.** The grid already shades by targeted-resource yield (§2). If
> shade and class fight each other visually, class wins — the yield number is on the readout
> anyway, and confidence is the thing with no other home. Render it before deciding.

### Phase 9 — Access *(designed in shape, not in detail — see §9)*

> **This phase is deliberately loose.** [excavation-design.md §2](excavation-design.md#depth-costs-access)
> settles *what* access is and *why* it belongs; it does not settle the numbers, the
> ownership, or the UI. Treat what follows as the shape of the work, not a build order to
> follow blindly. **Do not start Phase 9 until §9's open questions are answered** — most of
> them change the data model, not just the tuning.

The rule: a spot at depth `d` is workable only if connected, by strip or by shaft.

**9a — Strip. The cheap half, and it is nearly free.**

Phase 4 already shipped `SubCell::workedFraction[4]` and `HasBeenDug(int)`
(`prospecting_types.h:116-122`). Strip connectivity is a read over state that exists:

```cpp
// a spot is stripped when every layer above it in its column is worked out
bool IsStripped(const SubCell& c, int depth)
{
    for (int d = 0; d < depth; ++d)
    {
        if (c.workedFraction[d] < STRIP_COMPLETE_FRACTION) return false;
    }
    return true;
}
```

One constant (`STRIP_COMPLETE_FRACTION` — is a 90%-worked layer stripped, or must it be
100%?) and one predicate. No new state, no cross-module change. **9a could ship on its own**
and would already make depth cost something.

**9b — The shaft. Where the real work is.**

Everything here is genuinely undecided, so it is listed as questions rather than steps:

| Piece | What is unclear |
|-------|----------------|
| Where it lives | A `Shaft` struct on the excavation facade, or state on the grid? Per unit or per sect (§9)? |
| What it opens | Design says the 3×3 centred on it, down to a chosen depth. Untested against the cluster sizes the generator actually produces — a 3×3 may cover a whole ore body, or a corner of one |
| What it costs | CONSTRUCTION_MATERIALS + build days + standing power. All three scale with depth **faster than linearly**. No numbers yet, and there should not be until Phase 7's real data exists |
| How it is built | A build queue on the panel, or an instant spend with a timer? Nothing in the module has a build queue today |
| How it renders | A mark on the grid plus its 3×3 footprint, per the §5 mock-up. Needs a preview pass |
| Whether it can be undone | Design leans toward demolish-for-partial-refund on a long timer. Not decided |

**9c — Gating the dig.** Wherever `dig_engine` accepts a target, it also has to reject an
unconnected one, with a reason the panel can show (*"2 layers above"* / *"no shaft in
range"*). Small, but it is the change that makes 9a and 9b matter — engine-implemented is
not player-reachable.

**Why access is worth the trouble.** Without it, excavation and prospecting make the same
decision with a different verb — *click the spot with the best number.* A drill hole is a
needle and goes anywhere; a working face is a volume and must connect to the surface. Access
is the one mechanic excavation has that prospecting structurally cannot. It also gives
surveying a second and much sharper reason to exist: surveying to pick a spot is worth
+33–130%, while surveying to **site a shaft** is worth the whole build.

---

## 4. Sequencing Notes

```
Phase 1 ─▶ 2 ─▶ 3 ─▶ 4 ─▶ 6 ─▶ 7
                 └──▶ 5 (can run parallel to 4 and 6)

                            prospecting C1 ─▶ 8
                                     4 ─▶ 9a ─▶ 9c
                                          9b ─▶ 9c   (blocked on §9)
```

- **Phases 1–3 change no other module.** If the plan stalls, everything up to here is still
  a net improvement over the current multiply chain.
- **Phase 4 is the only cross-module change.** It touches `src/Prospecting/`, so it should
  be a separate commit, reviewed on its own, and coordinated if the prospecting branch is
  still active.
- **Phase 5 can start once Phase 3 produces real numbers** — it needs data to render, not
  the write-back.
- **Phase 8 waits only on prospecting C1**, which is a few lines. It is the cheapest
  remaining win in the module.
- **Phase 9a can ship alone.** It reads Phase 4's `workedFraction` and needs nothing new,
  so depth starts costing something long before shafts exist.
- **Phase 9b is not ready to build.** Its open questions in §9 change the data model, so
  starting it early means writing it twice. 9a first, then answer §9, then 9b.

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
| **`colony_measure_clusters`** | **Ore body footprint on the real lattice.** Best-placed N×N capture, rich-ground bounding boxes, and which variation clamp actually binds. Run it before changing `SUBCELL_VARIATION_*` or the shaft footprint |
| **`colony_sim`** | **A playtest that runs in CI.** Runs the real engines for 20 game days under five different players and asserts the design's orderings |

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
| **Access turns into busywork** — every dig needing a setup step | 9a is free (reads existing state) and dormant at T0; AUTO strips or proposes a shaft so a player who ignores access still plays a complete game |
| ~~**The shaft's 3×3 is the wrong size**~~ | **Retired.** Measured: 3×3 opens 30.8% of a field, bodies average 4.8×4.8, so a shaft opens a door rather than taking the body |
| **Two colour channels fight on the grid** — yield shade vs class | Render both before deciding; class wins if they clash (Phase 8) |

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

---

## 9. Open Before Phase 9b (Access)

**These change the data model, not just the tuning.** Phase 9b should not start until they
are answered — building it first means building it twice. Phase 9a (strip) depends on none
of them and can proceed.

**Ownership**
- `[?]` Is a shaft **per unit** or **per sect**? Per unit is simpler and keeps each
  extraction unit self-contained. Per sect makes siting a settlement-level decision and lets
  two units share one way in — more interesting, and a bigger change. *Leaning per unit
  unless a reason appears.*
- `[?]` Does the shaft live on the excavation facade or on `ProspectingGrid`? The grid is
  shared, which argues for the facade; but the grid is also what persists, which argues the
  other way.

**Shape**
- ✅ **The footprint is 3×3.** Measured with `colony_measure_clusters` over the full
  population — 400 planet cells × 4 depths × every resource, 8,948 fields at the standard
  seed. A best-placed 3×3 opens **30.8%** of a field's yield from **14%** of the lattice
  (2.2× concentration); 4×4 opens 47.2% and starts to solve the body rather than open it.
  Rich ground averages 15 of 64 sub-cells in a **4.8 × 4.8** box and fits inside a 3×3 only
  **7%** of the time, so a shaft is a way *in*, never a way to take the whole thing. Siting
  well is worth ~2.2× over siting at random. Details in
  [excavation-design.md §2](excavation-design.md#depth-costs-access).
- `[?]` Does a shaft open its footprint at **every** depth down to its own, or only at its
  terminal depth? Every depth is more forgiving and probably right.
- `[?]` Can shafts be built outside excavation's reach ring? Consistency with §2 says no.

**Economy**
- `[?]` Is stripped material **kept**? The design's table assumes yes. If it were not,
  stripping would be pure cost and shafts would always win — which collapses the choice.
- `[?]` `STRIP_COMPLETE_FRACTION` — is a 90%-worked layer stripped, or must it be 100%? 100%
  is cleaner to explain; 90% avoids a frustrating last sliver.
- `[?]` Does the standing power draw scale with depth, with footprint, or neither?
- `[?]` Can a shaft be abandoned or moved? Leaning: demolish for a partial materials refund
  on a long timer, so a bad siting is expensive rather than permanent.

**Interface**
- `[?]` Does the module need a **build queue**, or is a shaft an instant spend plus a timer?
  Nothing in excavation has a queue today, and adding one is a bigger change than it looks.
- `[?]` How does the panel show *why* a spot is unavailable? *"2 layers above"* and *"no
  shaft in range"* are different problems with different fixes, and the readout has one line.

**Balance, and therefore last**
- `[?]` Every shaft cost figure. Per the module's own rule, calibrate against dumped data in
  Phase 7 rather than guessing now.
