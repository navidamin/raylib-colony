# Module Design & Architecture Guide

How to design and structure a unit module — from first idea to shipped code.

Read this when starting **any** new module or unit type. Part I is a design
brief to work through *before writing code*; Part II is the implementation
shape; Part III is the checklists.

The prospecting module (`src/Prospecting/`) is the reference implementation
and the source of most examples, but the framework is deliberately
unit-agnostic. Where a section shows worked examples for Farming, Energy,
Manufacture, or Research, those are **illustrative sketches, not decided
design** — they exist to show the questions applied, and should be replaced
by real answers when those modules are designed.

---

# Part I — Design first

Work through these thirteen aspects before writing code. Answering them late
is what produces modules that are technically complete but not fun, not
legible, or impossible to scale.

Capture the answers in `docs/design/<module>/README.md`.

## The three project principles

Every module should satisfy the principles established by the prospecting
design (`docs/design/prospecting/prospecting-master-design.md` §1):

1. **Scientifically coherent** — mechanics map to real technologies and
   processes. Prospecting uses GPR, core drilling, XRF, LIBS, fire assay.
   A farming module should use real agronomy; manufacturing, real process
   engineering. Coherence gives players intuition to reason with, and gives
   you a source of mechanics that already make sense together.
2. **Multi-scale control** — every stage works automatically at reduced
   efficiency, *or* the player can fine-tune it for better results.
3. **Progressive disclosure** — complexity reveals itself as tiers unlock,
   not all at once.

## 1. Identity — what does the player *do*?

State the module's verb in one sentence. "The player surveys ground to learn
what is beneath it before committing extraction effort."

If the sentence needs an "and" it may be two modules. If you cannot write it
without describing the UI, the mechanic is not defined yet.

Also state the **fantasy**: what does it feel like to be the person doing
this? Prospecting feels like a geologist: uncertainty, instruments,
inference.

## 2. The loop — stages, inputs, outputs

Break the verb into sequential stages. Each stage must **produce something
the next stage consumes**:

```
Prospecting:  Sweep ──▶ Sample ──▶ Lab ──▶ survey progress
              (signal)  (samples)  (confidence)
```

For each stage record: what it consumes, what it produces, what it costs,
how long it takes, and what decision the player makes in it.

**Test**: if a stage's output does not change what happens downstream,
delete the stage.

## 3. The contract — what crosses the module boundary

Define, before building, exactly:

- **Exports** — what the rest of the game reads from this module.
  Prospecting exports two values: `GetSurveyProgress()` and
  `IsMarkedSite()`. That is the entire surface.
- **Imports** — what it needs from the world (`ResourceManager` grid data,
  game time, unit storage).

**Keep exports as narrow as you can defend.** A narrow contract is what let
prospecting be rewritten from scratch — five engines, four depth layers, a
lab pipeline — without touching the extraction formula that consumes it.

## 4. Data & units

- What are the **entities**? (`Sample`, `SubCell`, `SweepRecord`)
- Where does **world data** come from, and in what units?
- What is **ground truth** vs what the **player has learned**? Prospecting
  separates `trueComposition` from `elementConfidence` — the player's
  knowledge is a distinct, growing thing. Any module with uncertainty needs
  this split.

**Name the unit of every value that crosses a boundary** — quantity vs
fraction vs rate vs per-tick. See Part II §2; this is the single most
expensive mistake made in this codebase so far.

## 5. Multi-scale control — the engagement spectrum

Principle 2, made concrete. For **each stage**, define both ends:

```
[DEFAULT / AUTO]                         [FINE CONTROL]
AI handles decisions              Player makes each choice
Lower efficiency                  Full efficiency potential
No per-item decisions             Per-item optimisation
```

Record a table like prospecting's:

| Stage | Default behaviour | What it misses | Penalty type |
|---|---|---|---|
| Sweep | Surface-only, single frequency | Deep anomalies | Missed opportunity |
| Sampling | Random cells, fixed depth order | Targeted sampling | Missed opportunity |

Prefer **missed opportunity** penalties over punishment: default mode should
be *worse*, not *broken*. A player who ignores this module should still
function; a player who engages should be rewarded.

Mode switching is per-stage and implicit: interacting with a stage overrides
auto for that stage.

## 6. Progression — what each tier *unlocks*

Tiers should unlock **capability**, not just multiply numbers. For each tier
0–3 state what becomes possible:

| Tier | Prospecting example | Pattern |
|---|---|---|
| 0 | Surface only, 1 frequency band, visual inspection | crude, manual, narrow |
| 1 | +1 depth layer, +bands, XRF | first real instruments |
| 2 | +depth, multi-tool lab, larger grid | breadth and choice |
| 3 | All depths, fire assay, auto-calibration | precision and automation |

The arc is **crude/manual → precise/automated**. Progressive disclosure means
a tier-0 player sees a simple panel; complexity appears as it unlocks.

Also decide: what is **per-unit** (tier) vs **colony-global** (research)?
Prospecting tiers are per-unit; AI capabilities are global.

## 7. Economy — costs and supply

- What does each action **cost**? (energy, time, materials, wear)
- What is **scarce**, and what is the tension that scarcity creates?
- Where does the resource **come from**? A cost with no income is a wall, not
  difficulty.

**Every cost must be charged, gated before commit, and refunded on failure**
— see `docs/guides/feature-completeness.md` §4–5.

State the intended pressure explicitly: prospecting's is *you cannot
exhaustively analyse everything, so choose which samples deserve depth*.

## 8. Friction & failure — what degrades, and the release valve

Interesting systems resist you. Decide:

- What **degrades** with use? (calibration drift, equipment wear)
- What can **fail** or be wasted? (destructive assays, spoilage)
- What is **finite**? (tray slots, one sweep per band per grid)

Then, for each: **what is the release valve?** Recalibrate, discard, repair,
reset. A degrading resource with no restore path, or a filling container with
no discard, is a dead end — this exact pair shipped in prospecting and made
the module unplayable until fixed.

## 9. Time — when do things happen?

- **Instant**, **timed** (blocks the instrument), or **cooldown**?
- What happens while the player is **not looking**? Most modules run
  per-tick via `Unit::Update`; decide whether yours accrues, idles, or
  requires attention.
- Does anything need to survive the player leaving the panel?

Timed actions need progress feedback and should genuinely block something —
calibration takes 30s and disables sweeping, which is what makes "when do I
recalibrate?" a decision.

## 10. Decision texture — where is the interesting choice?

For each stage, name the **tradeoff**. Real examples:

- *Which frequency band?* — depth vs resolution vs energy
- *Which depth to drill?* — cost vs what is buried there
- *Which tool on which sample?* — element coverage vs cost vs destructiveness

> **Test**: if an action has an obviously correct answer every time, it is a
> button, not a mechanic. Either give it a tradeoff or automate it away.

## 11. Scale — does it survive multiplication?

The player will eventually own many sects. For each interaction ask: *is this
tolerable ×20?*

- Per-unit micromanagement must be **optional** (multi-scale control) or
  **automatable** (AI tree).
- Anything mandatory and repetitive becomes tedium at scale.

This is the main reason default/auto mode is a principle rather than a
feature.

## 12. Feedback & hero visual

- How does the player see **current state** at a glance? (bottom-bar
  segments, progress bars, coloured thresholds)
- Is there a **hero visual** — the thing the player inspects and watches
  change? Prospecting has crystal samples; Manufacture would have part
  blanks; Energy, battery cells.
- How does the player know they are **doing well**?

Hero visuals are keyed by a small struct and tinted at runtime — see Part II
§5 and `docs/design/ui/sprite-manifest.md`.

## 13. AI automation hook

Every unit type is expected to get an AI research tree. Read
`docs/design/ai-automation/README.md` **before** finalising the design, and
sketch which decisions the AI would eventually make — free baseline,
convenience, intelligence, mastery.

Designing with this in mind prevents mechanics that cannot be automated
sensibly later.

## Worked example — the framework across unit types

Illustrative only, to show the questions applied beyond prospecting:

| Aspect | Prospecting (built) | Farming (sketch) | Manufacture (sketch) |
|---|---|---|---|
| Verb | survey ground before extracting | grow and harvest food | turn materials into components |
| Stages | sweep → sample → lab | prepare → grow → harvest → store | fabricate → assemble → QC → ship |
| Exports | survey progress, marked sites | food output, spoilage rate | component output, quality |
| Ground truth vs knowledge | composition vs confidence | soil state vs last survey | tolerance vs inspection result |
| Scarce | energy, tray slots | water, growing area | materials, line capacity |
| Degrades | calibration | soil fertility | tool wear |
| Release valve | recalibrate, discard | fallow/fertilise | maintenance |
| Tradeoff | band/depth/tool choice | crop mix vs water | throughput vs quality |
| Hero visual | crystal samples | crop growth stages | part blanks |

---

# Part II — Implementation shape

## 1. Directory & separation

```
src/<Module>/
  <module>_constants.h      tier tables, costs, tuning constants
  <module>_types.h/.cpp     data structs
  <module>_grid.h/.cpp      world-data adapter (if spatial)
  <stage>_engine.h/.cpp     pure logic, one per stage -- no rendering, no input
  <module>_system.h/.cpp    facade: owns engines + UI state
```

**Engines** are pure logic: data in, results out, no knowledge of rendering
or input. **The facade** owns the engines and the module's UI state (active
tab, selection, last-action timestamps) and is what the renderer talks to.
`Unit` owns the facade as a `unique_ptr`, created when the module exists.

The renderer is immediate-mode and keeps nothing between frames, so anything
that must persist lives on the facade. This also lets the preview and
playtest harnesses drive the module into any state without the renderer.

Add all sources to `COLONY_CORE_SOURCES` in `src/CMakeLists.txt`.

## 2. Declare units at subsystem boundaries

**The most expensive bug in this codebase so far.**

`ResourceManager` stores **absolute quantities** (hundreds to thousands per
cell). The prospecting chain assumed **composition fractions** (0–1). Nothing
objected — both are `float` in a `std::map<ResourceType, float>`. Richness
pinned at 100% for every sample; compositions rendered as `-36104%`.

> When a value crosses a subsystem boundary, **normalize at the boundary and
> name the unit in the API**.

```cpp
// Composition fractions (0-1, sum to ~1) -- "what this is made of"
std::map<ResourceType, float> GetGroundTruth(int subX, int subY, DepthLayer d) const;

// Absolute deposit quantity -- "how much is there"
float GetQuantity(int subX, int subY, DepthLayer d) const;
```

Consumers then choose deliberately: sweep signal uses **quantity** (fractions
sum to 1 everywhere and would flatten the heat map); richness uses
**quantity**; displayed composition uses **fractions**.

This recurs in every unit — crop yield vs nutrient mix, charge vs capacity,
part mass vs material ratio.

Normalization constants must be calibrated against **dumped real data**, and
should say so:

```cpp
// ResourceManager produces roughly 5,000 for an average cell layer and
// ~13,000 for a cluster core, before sub-cell variation (0.3x-2.0x).
constexpr float RICHNESS_NORMALIZATION = 10000.0f;
```

## 3. Tier gating

Capability lives in constant tables, not branching logic:

```cpp
constexpr int   MAX_DEPTH_PER_TIER[]      = { 1, 2, 3, 4 };
constexpr int   MAX_SWEEP_BAND_PER_TIER[] = { 0, 1, 2, 3 };
constexpr float DRILL_ENERGY_COST[4][4]   = { ... };   // [tier][depth]
```

Engines answer capability questions (`CanSweep`, `CanDrill`, `CanApplyTool`);
the UI asks rather than deciding. Tier changes propagate through one facade
method (`SetTier`) that updates every engine.

`Unit::DebugUpgradeModuleTier()` bypasses tech and resource costs — the
preview and playtest tools use it to reach high tiers without an economy.

## 4. Narrow contract, cached derivation

Expose the minimum. Cache expensive derived values behind it:

```cpp
mutable CellSurveyResult cachedResult;
mutable bool cacheValid = false;      // invalidated by any mutable accessor
```

Weight multi-stage contributions so every stage matters:

```cpp
constexpr float SURVEY_SWEEP_WEIGHT   = 0.20f;
constexpr float SURVEY_SAMPLE_WEIGHT  = 0.50f;
constexpr float SURVEY_TESTING_WEIGHT = 0.30f;
```

If a stage contributes nothing measurable, players will skip it — and you
have built a decorative UI, not a mechanic.

## 5. Hero visuals

Key the visual by a small struct whose fields index the sprite tree:

```cpp
struct CrystalVisual
{
    ShapeFamily shapeFamily;   // from depth layer
    int templateIndex;         // 0-4 within family
    int glowLevel;             // from confidence
    int sizeLevel;             // from richness
    Color elementColor;        // runtime tint from dominant element
};
```

Colour is a **runtime tint**, so one file set serves every material — adding
an element costs no art. Render through a **lazy texture cache** keyed by
path, unloaded in `RenderManager`'s destructor.

> **raylib gotcha**: draw calls are batched. Unloading a texture immediately
> after `DrawTexturePro` deletes it before the batch flushes and draws
> garbage. Load everything for a frame first; unload after `EndTextureMode`.

## 6. Multi-stage panels

Give each stage a tab rather than one crowded panel (prospecting: **Sweep →
Samples → Lab**). Build the panel per `docs/guides/ui-panels.md`.

---

# Part III — Checklists

## Design (before code)

- [ ] Verb and fantasy stated in one sentence each
- [ ] Stages defined, each producing what the next consumes
- [ ] Exports/imports listed — exports as narrow as defensible
- [ ] Entities defined; ground truth separated from player knowledge
- [ ] Units named for every boundary value
- [ ] Default/auto behaviour defined per stage, with penalty type
- [ ] Tier 0–3 capability arc (crude/manual → precise/automated)
- [ ] Costs, scarcity, and income source identified
- [ ] Degradation and its release valve for every finite/decaying thing
- [ ] Timing model (instant / timed / cooldown; unattended behaviour)
- [ ] A named tradeoff per interaction — no obviously-correct answers
- [ ] Interactions tolerable ×20 sects, or optional/automatable
- [ ] Hero visual and at-a-glance state feedback identified
- [ ] AI automation sketched against `docs/design/ai-automation/README.md`
- [ ] Written up in `docs/design/<module>/README.md`

## Implementation

- [ ] `src/<Module>/` with constants / types / engines / facade
- [ ] Engines contain no rendering or input code
- [ ] Facade owns engines + UI state; `Unit` owns the facade
- [ ] Boundary APIs name their units; constants calibrated against real data
- [ ] Tier capability in constant tables, queried via `Can*()`
- [ ] Sources added to `COLONY_CORE_SOURCES`
- [ ] Panel built per `docs/guides/ui-panels.md`
- [ ] Verified against `docs/guides/feature-completeness.md`
