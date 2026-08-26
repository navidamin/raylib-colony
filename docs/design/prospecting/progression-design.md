# Prospecting — Progression: Rigs and Techniques

> Status: **DESIGN** — plan of record for progression; not built
> Last Updated: 2026-08-26
> Parent: [README.md](README.md)
> Pairs with: [block-model-design.md](block-model-design.md) (the loop this progresses)
>
> **Supersedes every `*_PER_TIER` table in `prospecting_constants.h` and the
> tier arc that used to live in [block-model-design.md §4](block-model-design.md).**
> Excavation's own tier is untouched by this document — see §8.

---

## 1. Why the Tier Is Dropped

Two decisions came first, and this document is where they land coherently:

1. **Depth is not gated.** All four layers are drillable from the first
   minute. With a four-layer block model as the central visual, locking three
   layers leaves three quarters of the screen dead — and "your drill cannot
   go deeper" is the least interesting constraint available: binary,
   unarguable, nothing to play against.
2. **The same argument kills prospecting's reach ring.** `{2,4,6,8}` gave
   tier 0 four sub-cells — four possible holes in the entire tier. Identical
   species of constraint: a wall where a price should be. Deleted. Tier 0 goes
   from 4 live blocks to 256.

With depth and reach gone, what was left of the tier was a bundle: tray size,
drill discounts, sweep noise, all moving in lockstep behind one integer. That
shape has three faults no ladder ever escapes:

- **Lockstep bundling.** You can never want *one* thing; every upgrade is
  everything at once, so no purchase expresses a plan.
- **Strict domination.** Tier N+1 is simply better, so there is nothing to
  weigh — an upgrade is a purchase, not a decision.
- **No fiction.** "Tier 2" is a stat. Nothing on screen changes but numbers.

The replacement splits progression into two currencies that are different in
kind — and it is not a novelty here: **excavation already made this exact
move.** Its design has no "digger tier 2"; it has six named machines with
genuinely different strengths, because on the Moon a digger cannot push
([excavation-design.md §4](../excavation/excavation-design.md)). Prospecting
adopting the same shape makes the two halves of the extraction unit rhyme,
and lets the panel reuse the machine-bay card UI that already shipped.

---

## 2. The Frame

| | **Rigs** | **Techniques** |
|---|---|---|
| What they are | physical instruments, drawn at the collar while they work | knowledge — ways of working and of concluding |
| How you get them | built/bought with resources, mounted on the unit | unlocked through `UnlockRegistry` (what the Research unit sells) |
| Scope | per unit, limited mounting slots | colony-wide, permanent |
| Shape | a **stable, not a ladder** — none strictly dominates | prerequisites form a short chain per axis |
| What they gate | what a hole *is* and *what comes back up it* | what you may *conclude* from it, and how you may *aim* |

The three axes from the progression review map cleanly onto the split:

```
RECOVERY  (what comes back up the hole)   = which rig
AIM       (where you may point it)        = rig capability × technique
CONTINUITY (how far one core may speak)   = technique only
```

The old T0–T3 rungs survive as *content* — they just stop being a single
staircase and become things you pick between.

---

## 3. The Rig Stable

Five rigs plus one starter. **Numbers are placeholders to be calibrated
against dumped data** (the repo's standing rule); the *relationships* are the
design. Energy is per metre and per layer — the per-metre cost model in
[block-model-design.md §4](block-model-design.md) — and is **never discounted
by anything**, which is what keeps "bank energy and drill later" from ever
being correct.

| Rig | Metres / energy | What comes back | The catch |
|-----|-----------------|-----------------|-----------|
| **Auger** (starter, free) | good in regolith, terrible below it | true core — certain along the trace | vertical only; slow setup |
| **RC percussion** | ~3× the diamond rig | chips: grade ±20%, support **capped at 0.75** | can reach Indicated, can **never** produce Measured |
| **Diamond core** | 1× (the reference) | true core — Measured along the trace | dear and slow |
| **Wireline add-on** | — | re-enter a finished hole; wedge daughter holes off it, paying only metres past the kick-off | mounts on the diamond rig only |
| **Orientation tool** | — | each intercept also reports the local contact attitude ±15° — the surface the bit cut, never the body's axis | needs core; useless on RC chips |

Why this is a stable and not a ladder: the RC rig never retires, because
cheap wide doubt is always worth having. The permanent decision — rated the
strongest of the whole review — is per hole: **am I buying area or
certainty?** RC ground commits at the Indicated discount and can never be
certified; core ground pays full and is certain. Neither answer dominates.

**Mounting slots** (2 at start, +1 buildable) are what make the stable a
loadout: which pair you carry changes per site, and there is no correct
answer to copy from a wiki.

**The prospecting unit's module card pips** stop meaning tier and show
**mounted rigs** — UI continuity with no lie in it.

---

## 4. The Technique Chain

Unlocked via `UnlockRegistry` tech strings, same mechanism the module tiers
already checked (`Spectroscopy`, `Geophysics`, `MechanizedDrilling` exist
today). New strings, grouped by the axis they serve:

| Technique (tech string) | Axis | What it unlocks | Replaces old rung |
|---|---|---|---|
| `AngledCollaring` | AIM | dip detents {90°, 67°, 45°}, 8 compass points | T1 |
| `DirectionalDrilling` | AIM | continuous azimuth and dip; target any block | T2 |
| `CorroboratedLogging` | CONTINUITY | two agreeing cores raise support along the segment between them | T1 |
| `StructuralInterpretation` | CONTINUITY | declare a strike; the estimate corridor stretches along it, **volume-conserved 3:1:1** | T2 |
| `AnisotropicModelling` | CONTINUITY | strike *and* dip, ratio to 5:1, declared **per element**; leave-one-out misfit readout. No auto-fit, ever | T3 |
| `CompositeDrilling` | RECOVERY | switch method mid-hole: RC pre-collar through barren overburden, core through the target | T3 |

Two properties carried over from the review, both deliberate:

- **The same belief, priced twice.** `DirectionalDrilling` spends metres on an
  orientation guess and fails by returning barren core — cheap, local,
  immediate. `StructuralInterpretation` spends nothing and fails by
  manufacturing committable tonnage a later hole retracts — free, wide,
  delayed. One UI control: the declared azimuth draws as a ghost line across
  all four plates and the aim widget snaps to it.
- **Techniques gate conclusions, never access.** Nothing on this list stops a
  hole being drilled. That is what makes every one of them pass the
  "just wait" test: the ground is always open; what grows is what you can
  honestly say about it.

---

## 5. How Depth Stays Meaningful With Nothing Gated

Four mechanisms, none a permission:

1. **Price per metre, never discounted.** `ENERGY_PER_METRE ≈ {1.2, 1.9,
   2.8, 4.0}` per layer: a full 120 m column runs ~366 E against ~14 E for
   the regolith alone. "Is this anomaly worth my whole cycle" is an argument
   you have with yourself, not a greyed-out chip.
2. **Deep is thin by geometry.** Layer centres sit at ~6/23/51/94 m; at
   RANGE = 20 m the cross-layer support terms are 0.49 / 0.14 / 0.01. A
   surface core says almost nothing about intact bedrock — arithmetic, not a
   rule.
3. **Deep is hardest to reach obliquely.** A 30° hole needs ~240 m of trace
   to bottom the column, so the along-strike play on a *deep* shoot is a
   late ambition priced in metres — while a vertical hole reaches the bottom
   plate from minute one.
4. **Deep is hardest to corroborate.** Corridors need pairs, and two deep
   holes cost what six shallow ones do.

---

## 6. Constant Disposition Table

The implementable core: what happens to every tier-indexed constant.

| Constant | Disposition |
|----------|-------------|
| `PROSPECTING_REACH_PER_TIER` | **Delete.** Prospecting reach is ungated; `ProspectingGrid::IsInReach` returns true. The free `IsSubCellInReach(x,y,tier)` **stays** — excavation reads it with its own tier |
| `MAX_DEPTH_PER_TIER` | **Delete.** All layers generated and drillable from the start; also deletes the `ResizeForTier` regeneration hack that existed only because of it |
| `MAX_SWEEP_BAND_PER_TIER`, `SWEEP_BLUR_PER_BAND`, band arrays | **Delete.** GPR is gone; LIBS is single-mode |
| `SWEEP_NOISE_PER_TIER` | **Delete.** LIBS noise is a property of LIBS |
| `TRAY_BASE_CAPACITY` | **Repurpose.** Once assays live permanently on `SubCell` (prerequisite #2 below), the tray caps *physical specimens only* — a cosmetic shelf, possibly rig-dependent, never a cap on knowledge |
| `LAB_BENCH_SLOTS` | **Delete** with `LabEngine` (already orphaned) |
| `DRILL_ENERGY_COST[tier][depth]` | **Replace** with `SETUP_ENERGY_COST[rig] + Σ RIG_ENERGY_PER_METRE[rig][layer] × metres`, using `LAYER_THICKNESS_M = {12, 22, 34, 52}` |
| `UpgradeModuleTier()` tier deps | **Becomes** rig construction costs + technique unlock checks against the registry |
| Module card tier pips | **Show mounted rigs** |

### Prerequisites (from the progression review — verified in code, unchanged by this frame)

1. **Believed-grade relief.** The block model's height channel currently draws
   ground truth (`rendermanager.cpp`, `GetSubCellYield` for every block).
   Until relief shows what you *know*, no information progression means
   anything. ~10 lines via `EstimateEngine::EstimateAt` as a stopgap.
2. **Assays live on `SubCell`, permanently.** `GetDepthConfidence` reads the
   tray, so evicting a core currently *deletes the ground it classified*.
3. **Indicated commits at a discount** (~0.75×). Today `IsCommittable()` makes
   Measured and Indicated mechanically identical, so the RC rig's support cap
   would cost nothing and RC would strictly dominate core. Load-bearing.
4. **Per-metre cost model** (`LAYER_THICKNESS_M` + the table above).
5. **Sweep normalisation made absolute** before reach opens — the current
   max-normalise and mean+σ anomaly cut both silently change meaning when the
   in-reach set changes.

---

## 7. Rejected Frames, For the Record

- **Mastery-by-use** ("drill 20 angled holes, dip tolerance tightens") —
  seductive, fails the just-wait test in a new costume: the optimal play
  becomes junk holes to farm the counter. The real mastery curve already
  exists and is measured — a well-aimed hole beats a badly-aimed one by +90%
  — and the game should not duplicate it with an XP bar.
- **Crew / specialists** (a geologist who carries techniques) — genuinely
  attractive, but there is no people system anywhere in the codebase;
  MANPOWER is a number. A whole new game system, not a progression reframe.
  Kept in the drawer for when population exists.
- **Time-priced axes** (parallel rigs, hole duration, commitment windows) —
  rejected wholesale by the review: `CollectSample` is synchronous and
  prospecting's clock is advanced by the *renderer* (`GetFrameTime()` in the
  draw call), so anything time-based needs a scheduler first and would today
  stop working when the panel closes.

---

## 8. What This Does *Not* Touch

- **Excavation's tier.** Its ladder (machine count, its own reach, AI levels)
  is out of scope here. Note for later: excavation's machines already *are*
  the instrument frame, so when its tier dissolves it dissolves the same way
  — but that is [block-mining-design.md](../excavation/block-mining-design.md)'s
  problem, after prospecting v2 lands.
- **The v2 dependency order.** Rigs/techniques change *what indexes the
  tables*, not the build order: oriented 3D shoots → estimate field →
  line holes. `DirectionalDrilling` and everything in CONTINUITY above
  `CorroboratedLogging` are dead letters until the ground has a shape —
  an angled hole through four independent 2D layers samples uncorrelated
  ground and aiming buys literally nothing.

## 9. Open

- `[?]` Slot count and the cost of the third slot — calibrate, don't guess
- `[?]` Can rigs relocate between units, or are they built in place?
- `[?]` Does LIBS itself have instrument grades (a better rover), or stay
  single? Leaning single — one wide instrument, flat, forever
- `[?]` Rig arrival as an *event* (delivery, salvage) rather than a silent
  purchase — cheap attractiveness if the colony sim ever has shipments
