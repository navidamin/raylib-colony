# Excavation Module — Design Documents

> **Auto-context rule:** When working on excavation-related code (`src/Unit/unit.cpp` `ProcessExtraction()` Stage 1 and excavator handling, `src/Engine/rendermanager.cpp` excavation panel, excavation input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [excavation-design.md](excavation-design.md) | **The design.** Panel + place + gamble + machinery, with the rules that keep the gamble honest | DRAFT |
| 2 | [implementation-plan.md](implementation-plan.md) | **How to build it.** Branch merge, architecture, 9 phases, testing, risks. Phases 1-6 done; 8 small; 9 (access) loose by design | DRAFT |
| 3 | [design-options.md](design-options.md) | Round 1 exploration: 16 aspects, then 4 options compared | SUPERSEDED |
| 4 | [design-options-v2.md](design-options-v2.md) | Round 2 exploration: three variants of the panel direction | SUPERSEDED |
| 5 | [excavation-mechanics.md](excavation-mechanics.md) | Science review of real excavation technologies (Part 1 is the reference; Part 3's A/B/C alternatives are superseded) | REFERENCE |
| 6 | [block-mining-design.md](block-mining-design.md) | **Mining the block model.** What changes once prospecting v2 gives the ground a 3D shape: access, selectivity, the empty-out. Deltas only — #1 still holds | SKETCH — blocked on prospecting v2 |
| 7 | [rebuild-plan.md](rebuild-plan.md) | **The full workflow.** What excavation becomes (prospecting's anatomy, none of its subject), the three things that make it not-prospecting, seven phases each leaving the game playable, and the traps collected up front | PLAN |
| 8 | [prototypes/diamond-drill.html](prototypes/diamond-drill.html) | **The shaft-sinker, playable.** Two variants of a diamond rotary rig on one depth scale, taking the same two commands. Argues the weight band, whirl-at-depth, and core-on-the-way-down | PROTOTYPE |

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
| **Place** | Which spot in the 32×32 lattice, and how deep. Reach is a tier ring, not a dial |
| **The gamble** | Digging without having paid to survey first — not random noise, a choice to skip a cost |
| **Machinery** | Six machines with different depth, precision, pace and wear. Optional; AUTO by default |
| **Access** | Deep ground must connect to the surface — strip the column, or sink a shaft that opens a 3×3 block. Tier buys lateral reach; access buys depth |

The first four lock together through one machine stat: **precision**. A sloppy machine digs
wider than you aimed and averages your chosen spot with its neighbours — throwing away the
survey you paid for. So *"should I survey?"* and *"which machine?"* become the same question.

Excavation invents no geography. Prospecting builds a **fixed 32×32** sub-cell lattice per
unit, with 0.3×–2.0× variation between spots, per-spot per-depth confidence, and per-spot
per-depth ground truth. Excavation reads that lattice and digs in it.

**Tier extends reach, not resolution** — concentric rings out from the sect,
8×8 → 16×16 → 24×24 → 32×32. Excavation reads that reach with its **own** tier, which is where the
gamble becomes structural: dig further than you can survey and you're working ground you have
no way to learn about first. At tier 0 the grid's best spot is inside reach only **7%** of
the time, so early game isn't about surveying — it's about being pinned to whatever lies
under the sect.

**Access is the vertical half, and it is what excavation has that prospecting cannot.** A
drill hole is a needle and goes anywhere; a working face is a volume and must connect to the
surface. A spot at depth is workable only once it's been stripped (every spot above it in the
column worked out) or a **shaft** has been sunk nearby, opening a 3×3 block down to the depth
you paid for. Siting that shaft is the module's highest-stakes decision, and since depth
re-rolls the ore clusters, siting one on unsurveyed deep ground bets the whole build rather
than one spot's machine time. At tier 0 the system is dormant — there is only one depth.

Confidence is presented as **three named classes** rather than a raw number — Measured /
Indicated / Inferred — on **one colour key shared by three surfaces**: the ring on the
resource icon, the four-layer depth map, and the excavation grid. Because the ring summarises
exactly the field the map details, the player can tell at a glance which elements are drilled
out and which are guesses without opening anything. That only works because colour is spent
on class; the element identifies itself through the icon, the tinted rock wall and the shape
of the relief instead.

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
| `src/Prospecting/prospecting_types.h` | `SubCell::aggregateConfidence`; `GetReachForTier`, `IsSubCellInReach`, `TierRequiredForSubCell` — excavation calls the reach helpers with its **own** tier |
| `src/Prospecting/prospecting_constants.h` | `PROSPECTING_GRID_SIZE` (fixed 8), `PROSPECTING_REACH_PER_TIER` (2/4/6/8), `SUBCELL_VARIATION_MIN/MAX` (0.3–2.0), `MAX_DEPTH_PER_TIER` |
| `src/game_constants.h` | `EXTRACTION_PRODUCTION_COSTS`, survey constants |
| `src/Excavation/excavation_constants.h` | `EXC_MACHINES` — the machine table the prototype's two variants are shaped to join |

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

### Prototypes

| File | What it is for |
|------|----------------|
| [`prototypes/diamond-drill.html`](prototypes/diamond-drill.html) | The **access rig**, drawn and driveable. Run it with `shot.js` for stills. Its argument: a diamond crown cuts only inside a *band* of weight that rises with hardness, so a light rig misses basalt's band entirely while a heavy one overshoots regolith — two machines, neither a strict upgrade, exactly as `EXC_MACHINES` requires. Raises one open question for §Access: if the shaft is sunk by a **coring** rig, the column it brings up is a survey, and gentle driving buys knowledge that hard driving does not |
| `prototypes/shot.js` | Headless capture. Poses are reached by *running* the sim at fixed dt, never by setting depth — a jumped depth leaves the core strip empty and the still lies about the run |

Style for both lives in [`../graphics/dark-plating.md`](../graphics/dark-plating.md)
§6.5 (the crown) — read it before changing any of the drawing code.

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

Full plan in [implementation-plan.md](implementation-plan.md). In short:

**`claude/game-status-remaining-z2u35f` is merged** (8b6e880, no conflicts). It carries the
prospecting work this design stands on — the fixed-lattice reach model, the
quantity/composition split, the rebuilt extraction UI, and the test instruments. Re-merge
before each phase; that branch is still moving.

Then seven phases, following `docs/guides/module-architecture.md`:

1. **Read-only adapter** — prove excavation can see prospecting's grid, no behaviour change
2. **Estimate engine** — the blur (Rules 1–2), stable-hashed, unit-tested
3. **Dig engine** — replaces the Stage 1 multiply chain; machine table; precision blending
4. **Write-back** — the only cross-module change; a single setter on `ProspectingGrid`
5. **The panel** — grid shaded by targeted-resource yield, machine bay, sliders, readout
6. **AUTO and the AI ladder** — Basic → Trained → Expert
7. **Balance** — every constant calibrated against dumped real data

Phases 1–3 change no other module, so the work is useful even if it stalls there.

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
