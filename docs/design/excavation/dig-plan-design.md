# Excavation Module — Dig Plan Design (Chosen)

> Status: DRAFT — approved direction, pending implementation
> Last Updated: 2026-08-13
> Parent: [README.md](README.md)
> Supersedes the alternatives in [excavation-mechanics.md](excavation-mechanics.md) Part 3

---

## 1. Decision Record

**Chosen: Design B ("Mine Plan") + minor machinery selection.**

The player decides **where to cut** (block-level dig plan over the prospecting sub-cell
grid) and **what to cut it with** (pick one machine from an unlocked list). The player does
*not* tune tool parameters — machines are presets. That is the "minor" part: machinery
selection is one meaningful choice, not a panel of sliders.

The central tension is **grade control vs dilution**: digging ground you haven't surveyed
does not merely reduce a multiplier, it pulls waste into the feed, so beneficiation does
the same work for less product.

**Development is split into two levels:**

| | Scope | When |
|---|---|---|
| **Level 1** | Self-contained playable layer. Block model, machine selection, dig plan, dilution, depletion, auto mode. Integrates with every existing system without requiring changes to them. | **Now** |
| **Level 2** | Depth physics, machine tuning, wear/maintenance, duty cycle, bench geometry, haulage, fleet roles. | After the surrounding modules (Energy, Manufacturing, Beneficiation redesign, prospecting sub-cell confidence) are plugged in |

### Level split policy

A feature belongs in **Level 1** if it satisfies all three:
1. It can be built without changing any module outside excavation.
2. It contributes to a complete player loop that works on its own.
3. Leaving it out of Level 2's way costs nothing later (see §5, Forward Compatibility).

A feature belongs in **Level 2** if it depends on a system that doesn't exist yet, or if it
adds pressure/depth to a loop that must already be working before the pressure means anything.

---

## 2. Level 1 — "Dig Plan" (build now)

### 2.1 Block Model

Each extraction unit owns a **5×5 sub-cell grid × 4 depth layers** = 100 blocks. The 5×5
grid is the same one prospecting Phase 1 defines, so the two modules address identical
ground.

```cpp
struct DigBlock
{
    std::map<ResourceType, float> grade;   // ground-truth abundance, 0.0-1.0
    float remaining = 1.0f;                // depletion, 1.0 = untouched
    float hardness = 0.0f;                 // 0.0-1.0, derived from depth layer
    bool  isPlanned = false;
    int   planOrder = -1;                  // execution order, -1 = unplanned
};
```

Blocks are populated on unit construction from `ResourceManager::GetResourcesAtGridLayer()`
plus a small per-block spatial variation so the 25 sub-cells are not identical — the
heterogeneity is what makes selectivity matter (§2.5).

**Confidence is deliberately not stored in the block.** It is queried live, so survey
progress made after planning immediately improves results.

### 2.2 Confidence Adapter (the seam to prospecting)

```cpp
float Unit::GetBlockConfidence(int sx, int sy, DepthLayer layer) const;
```

**Level 1 body:** returns the aggregate `prospectingSystem->GetSurveyProgress()` for every
block, plus `SURVEY_MARKED_SITE_BONUS` if the site is marked, clamped to 1.0.

**Level 2 body:** returns real per-sub-cell, per-layer confidence once the prospecting
redesign ships.

This single function is the entire coupling to prospecting. Excavation can be built,
balanced, and shipped before prospecting's sub-cell work exists, and gains its full depth
later with a one-function change. Nothing else in excavation reads prospecting directly.

### 2.3 Machinery Selection

The unit runs **one machine class at a time**. Machine count still comes from module tier
(existing behavior: T0=1, T1=2, T2=4, T3=8). Machines are data, loaded from
`game_types.toml`.

```cpp
struct MachineSpec
{
    std::string id;              // "scoop" | "bucket_wheel" | "bucket_drum" | "percussive"
    std::string displayName;
    float smuRadius;             // selectivity in blocks; <1.0 stays inside one block
    float rateMultiplier;        // throughput vs baseline
    float energyMultiplier;      // energy per kg vs baseline
    float wearMultiplier;        // wear accrual vs baseline
    int   maxDepthLayer;         // deepest DepthLayer index reachable
    int   requiredTier;          // module tier that unlocks it
    std::string requiredTech;    // UnlockRegistry key, empty = always available
};
```

| Machine | smuRadius | rate | energy | wear | max depth | tier | tech |
|---------|-----------|------|--------|------|-----------|------|------|
| **Scoop** | 1.4 (coarse) | 0.7 | 0.7 | 1.0 | SURFACE | 0 | — |
| **Bucket wheel** | 1.1 | **1.6** | 1.0 | 1.2 | SHALLOW | 1 | MechanizedDrilling |
| **Bucket drum** | **0.6** (fine) | 1.0 | 1.1 | **0.6** | SHALLOW | 1 | MechanizedDrilling |
| **Percussive** | 0.9 | 1.2 | **1.8** | 1.8 | **DEEP** | 2 | HeavyEquipment |

Grounded in the science review: the drum's counter-rotating design gives net-zero reaction
force, so it is precise and low-wear but not fast; the bucket wheel is the volume tool with
a scale penalty; percussion buys depth in hard ground at a steep energy and wear price
([mechanics §2–5, §8](excavation-mechanics.md#part-1-real-excavation-technologies--game-mechanics)).

**Wheel and drum both unlock at T1.** That is the first real choice in the module — fast
and dirty vs clean and reliable — and it has no dominant answer, because the winner depends
on how heterogeneous the ground is (§2.5).

### 2.4 Dig Plan

The player selects blocks on one depth layer at a time and they enter an ordered queue.
The active machine works `planOrder` 0 first, advances when a block is exhausted.

| Tier | Max plan size | Layers reachable | Notes |
|------|--------------|------------------|-------|
| T0 | 1 | SURFACE | Single-block manual dig |
| T1 | 3 | SURFACE, SHALLOW | Machine choice unlocks; dilution % shown |
| T2 | 6 | + MID, DEEP | Per-block dilution breakdown shown |
| T3 | 12 | all | Improved auto-planner |

Level 1 plans are **2D within one layer**. Bench sequencing across layers is Level 2.

### 2.5 Dilution — the core mechanic

Two independent sources, one clamped sum:

```
geometricDilution = DILUTION_SMU_WEIGHT
                  * clamp(machine.smuRadius - 1.0f, 0.0f, 1.0f)
                  * neighborhoodWasteFraction(sx, sy, layer)

knowledgeDilution = DILUTION_CONFIDENCE_WEIGHT
                  * (1.0f - GetBlockConfidence(sx, sy, layer))

dilution = clamp(geometricDilution + knowledgeDilution, 0.0f, DILUTION_CAP)
```

`neighborhoodWasteFraction` is the fraction of the 8 neighboring blocks whose total grade
is materially lower than this block's. It produces the rule that makes machinery selection
interesting:

> **A coarse machine costs nothing in uniformly rich ground, and ruins an isolated rich pocket.**

So the bucket wheel is correct on a broad mare deposit and wrong on a patchy one, and the
player can only tell which they have by surveying first. Machine choice, survey investment,
and site archetype all land on the same decision.

Applied to output:

```
deliveredGrade[r] = block.grade[r] * (1.0f - dilution)
```

**Mass moved is unchanged; only grade falls.** Energy spent per tick is the same. The
player moved the tonnage and got less ore for it — which is what dilution actually is, and
it reads clearly in the UI as one number.

### 2.6 Throughput and Integration with Existing Multipliers

`ProcessExtraction()` Stage 1 is replaced by a per-block computation. **Every existing
multiplier is preserved and keeps its current meaning**, so Operations, Directives, and
tier upgrades continue working with no changes:

```
blockMassPerTick = BASE_DIG_RATE
                 * machine.rateMultiplier      // NEW
                 * hardnessFactor(layer)       // NEW: 1.0 / 0.85 / 0.7 / 0.5
                 * tierMultiplier              // existing (1.0 / 1.4 / 1.9 / 2.5)
                 * activeMachineCount          // existing
                 * opsModifier                 // existing
                 * directiveModifier           // existing
                 * scanMultiplier              // existing (retuned, see below)
                 * deltaTime

rawRegolith[r] += blockMassPerTick * block.grade[r] * (1.0f - dilution) * block.remaining
```

Stages 2 and 3 (separation chain, sect storage) are untouched — they receive the same
`std::map<ResourceType, float>` type they receive today, just with different numbers in it.

#### Correction to the earlier README claim

The README states the survey formula stays intact. It stays intact in *shape*, but two
constants must be retuned, because `knowledgeDilution` and `scanMultiplier` would otherwise
penalize unsurveyed ground twice:

| Constant | Today | Level 1 | Rationale |
|----------|-------|---------|-----------|
| `SURVEY_UNSCANNED_EFFICIENCY` | 0.35 | **0.70** | scanMultiplier becomes a mild *rate* effect |
| `SURVEY_SCANNED_BONUS` | 0.65 | **0.30** | knowledge penalty moves to *grade* via dilution |

Net effect on fully-unsurveyed output is roughly preserved (0.35 today vs
0.70 × (1 − 0.40) ≈ 0.42), but the penalty now has a mechanism the player can see and act
on. This is a balance item to verify in play, not a settled number.

### 2.7 Depletion

Blocks deplete individually:

```
block.remaining -= blockMassPerTick / BLOCK_MASS_UNITS;
resourceManager.UpdateResourceDepletion(gridX, gridY, r, extractedAmount);  // existing call
```

When `remaining <= 0.0f` the block is exhausted and the plan advances. The pit therefore
migrates outward across the 5×5 grid and downward through layers over the colony's life,
which is visible in the UI without any extra bookkeeping.

`BLOCK_MASS_UNITS` sets how long a block lasts — start at `500.0f` (≈2 game days at T0) and
tune.

### 2.8 Default / Auto Mode

If the plan is empty, `AutoSelectDigPlan()` runs so the game plays without the player ever
opening the panel:

- Score each reachable block by `Σ(grade) × confidence × remaining`
- Take the top *N* for the tier's plan size
- Apply `AUTO_PLAN_EFFICIENCY = 0.85f` (0.95 at T3)

Same pattern and same penalty philosophy as the prospecting module's default mode.

### 2.9 UI (minimum for Level 1)

Inside the existing excavation panel, in the established dark-theme language:

- **5×5 grid**, cells heat-colored by expected grade, dimmed in proportion to `1 − confidence`
- **Depth layer tabs** (SURFACE / SHALLOW / MID / DEEP), locked ones greyed with the tier requirement
- **Machine row** — ghost buttons, one per unlocked machine, showing rate/precision/depth at a glance
- **Plan queue** — ordered list with block coordinates and progress bars
- **The teaching line:** `PLANNED 0.42 → DELIVERED 0.27  (−36% DILUTION)`
- **AUTO toggle**

That single readout line is the whole lesson of the module. Everything else is support.

### 2.10 Data

New `game_types.toml` section, following the existing `[modules.Excavation]` shape:

```toml
[modules.Excavation.machines.scoop]
display_name    = "Manual Scoop"
smu_radius      = 1.4
rate_multiplier = 0.7
energy_multiplier = 0.7
wear_multiplier = 1.0
max_depth_layer = 0
required_tier   = 0
required_tech   = ""

[modules.Excavation.machines.bucket_wheel]
display_name    = "Bucket Wheel"
smu_radius      = 1.1
rate_multiplier = 1.6
energy_multiplier = 1.0
wear_multiplier = 1.2
max_depth_layer = 1
required_tier   = 1
required_tech   = "MechanizedDrilling"

# ... bucket_drum, percussive
```

New constants in `game_constants.h`:

```cpp
const float BASE_DIG_RATE = 1.0f;                  // mass units per tick, baseline
const float BLOCK_MASS_UNITS = 500.0f;             // total yield of one block
const float DILUTION_SMU_WEIGHT = 0.5f;
const float DILUTION_CONFIDENCE_WEIGHT = 0.4f;
const float DILUTION_CAP = 0.75f;
const float AUTO_PLAN_EFFICIENCY = 0.85f;
const float AUTO_PLAN_EFFICIENCY_T3 = 0.95f;
const int   SUBCELL_GRID_SIZE = 5;
```

### 2.11 Integration Contract

| System | Change required |
|--------|----------------|
| Beneficiation (`ProcessExtraction` Stage 2) | **None** — same input type |
| Sect storage (Stage 3) | **None** |
| Operations module | **None** — `opsModifier` still applies |
| Directives module | **None** — all six directives still multiply in |
| Prospecting | **None** — read through `GetBlockConfidence()` adapter |
| ResourceManager | **None** — existing `GetResourcesAtGridLayer()` and `UpdateResourceDepletion()` |
| UnlockRegistry | **None** — machines use the existing tech-check pattern |
| `game_constants.h` | Two retuned survey constants + seven new constants |
| `game_types.toml` | New `[modules.Excavation.machines.*]` section |
| `unit.h` / `unit.cpp` | New structs and Stage 1 rewrite |
| `rendermanager.cpp` | New excavation panel content |

This is what "a layer that can be integrated with everything else" means concretely:
**no file outside excavation changes behavior.**

### 2.12 The Playable Loop (Level 1)

1. Open the excavation panel → see the 5×5 grid, mostly dim because it's unsurveyed
2. Pick a machine — at T0 there's only the scoop; at T1 the first real choice appears
3. Queue blocks; the readout shows a large dilution penalty
4. Notice delivered grade is far below planned grade
5. Go survey those blocks in prospecting
6. Return: the same plan now delivers much more, with no other change

That loop is complete, teaches the module's whole idea, and requires nothing from Level 2.

---

## 3. Level 2 — Deferred (build after everything is plugged in)

Each item lists what must exist first.

| Feature | From mechanics doc | Gated on |
|---------|-------------------|----------|
| **Bench sequencing + slope stability** — 3D plans across layers, angle-of-repose validation | §12 | L1 stable; prospecting per-sub-cell confidence shipped |
| **Continuous depth physics** — `density(depth)` → `cohesion(density)` → Balovnev force replacing the 4-layer `hardnessFactor` | §4, §8 | Depth stored as float end-to-end (already prepared, §5) |
| **Machine parameter tuning** — depth of cut, rake angle, advance rate; traction budget and slip (Design A's fine-control layer) | §1, §4 | L1 machine presets proven fun; tuning becomes the opt-in layer above them |
| **Force-driven wear + maintenance economy** — dual degradation tracks, scheduled maintenance costing MACHINERY | §10 | Manufacturing unit producing spares |
| **Power/thermal duty cycle** — DAY_OPERATION / NIGHT_SURVIVAL / THERMAL_HOLD | §13 | Energy unit redesign |
| **Conveyance backpressure + stockpile** — dig rate vs conveyance capacity as separate limits | §7 | Beneficiation redesign |
| **Haul distance, fleet roles, congestion** — DIG / HAUL / SUPPORT assignment, stockpile placement | §11 | T2+ fleet sizes; meaningless with 1–2 machines |
| **Ice-cemented ground class + thermal tool** | §9 | Polar archetype gameplay |
| **Pneumatic machine + gas budget** | §6 | Gas production chain exists |
| **Per-block AI planner with grade-vs-haul optimization** | §11, §12 | Haul distances exist |

Level 2 adds no new spine. Every item above attaches to a structure Level 1 already builds.

---

## 4. Build Order for Level 1

| Step | Work | Verifiable when |
|------|------|-----------------|
| 1 | `DigBlock` struct, 5×5×4 grid, population from ResourceManager | Blocks hold plausible per-sub-cell grades |
| 2 | `GetBlockConfidence()` adapter returning aggregate survey | Confidence changes as the player scans |
| 3 | `MachineSpec` loading from `game_types.toml`, tier/tech gating | Correct machines unlock at each tier |
| 4 | `ComputeDilution()` + `ComputeBlockThroughput()` | Unit tests: coarse machine in patchy ground loses grade, in uniform ground does not |
| 5 | Stage 1 rewrite in `ProcessExtraction()`, per-block depletion | Output still flows into beneficiation unchanged |
| 6 | Survey constant retune + balance pass | Unsurveyed output ≈ current baseline |
| 7 | `AutoSelectDigPlan()` | Game plays correctly with the panel never opened |
| 8 | Panel UI: grid, layer tabs, machine row, plan queue, dilution readout | The loop in §2.12 is playable |

Steps 1–7 are gameplay-complete without step 8 (auto mode covers it), so the UI can be
built and iterated separately.

---

## 5. Forward Compatibility Rules

Level 1 decisions that exist specifically to keep Level 2 cheap. Violating any of these
turns a Level 2 feature into a rewrite.

1. **Machines are TOML data, not an enum switch.** Level 2 adds tuning fields to
   `MachineSpec` without touching call sites.
2. **`DigBlock::grade` is a full resource map, never a scalar.** Dilution and beneficiation
   both need the vector.
3. **Store machine depth as a float `depthCm`; derive `DepthLayer` from it.** Level 2's
   continuous density/cohesion curve then drops in with no data migration.
4. **All dilution flows through one `ComputeDilution()` function.** Level 2 swaps the body.
5. **All throughput flows through one `ComputeBlockThroughput()` function.** Level 2 inserts
   the Balovnev force model and traction check there.
6. **Prospecting is read only through `GetBlockConfidence()`.** One function absorbs the
   entire prospecting redesign.
7. **Plan entries carry a layer field from day one**, even though Level 1 only ever plans
   one layer at a time. Level 2 bench sequencing then just populates it variably.
8. **Never collapse `plannedGrade` and `deliveredGrade` into one value.** The gap between
   them is the module's whole readout, and later features (haulage losses, conveyance
   spill) attach to that same gap.

---

## 6. Gaps

### Resolved by this document
- ✅ Which design → B + minor machinery selection
- ✅ Block resolution → reuse prospecting's 5×5 sub-cells, × 4 existing depth layers
- ✅ Dilution model → reduces delivered *grade*, not mass; no new waste `ResourceType`
- ✅ Depth representation → float on the machine, layer derived (L1 uses 4 buckets)
- ✅ Excavator addressability → fleet aggregate in L1; individually addressable in L2
- ✅ Double-penalty with `scanMultiplier` → resolved by retuning two survey constants
- ✅ Two-level split and the policy that decides which level a feature lands in

### Remaining for Level 1
- `[?]` Per-block grade variation: how much spatial noise around the cell average? Too little and selectivity never matters; too much and prospecting feels unreliable.
- `[?]` `BLOCK_MASS_UNITS = 500.0f` — needs a play pass to confirm block lifetime feels right.
- `[?]` Does the dig plan persist across save/load, or regenerate from auto-planner?
- `[?]` Is the dig plan painted in the excavation panel's own grid, or overlaid on the shared prospecting grid view?

### Remaining for Level 2
- `[?]` Do bench constraints hard-block illegal plans, or apply a stability risk penalty?
- `[?]` Does machine tuning replace the presets, or layer on top of a chosen preset?
