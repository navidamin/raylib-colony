# Module Architecture Guide

How to structure a unit module's gameplay code, and the data rules that keep
it correct.

Read this when starting a new module or unit. The prospecting module
(`src/Prospecting/`) is the reference implementation — it is the only module
built to this shape, and it is the template the others should follow.

---

## 1. The shape

Prospecting separates pure logic from UI and from the rest of the game:

```
src/Prospecting/
  prospecting_constants.h    tier tables, costs, tuning constants
  prospecting_types.h/.cpp   data structs (Sample, SubCell, CrystalVisual)
  prospecting_grid.h/.cpp    world data adapter (see rule 2 below)
  sweep_engine.h/.cpp        \
  sampling_engine.h/.cpp      |  pure logic, no rendering, no input
  lab_engine.h/.cpp           |
  survey_progress_engine.h/.cpp
  sample_tray.h/.cpp         /
  prospecting_system.h/.cpp  facade: owns engines + UI state
```

**Engines** are pure logic: they take data in, return results, and know
nothing about rendering or input. They are the part you can reason about and
test in isolation.

**The facade** (`ProspectingSystem`) owns the engines, exposes them via
accessors, and holds the module's UI state (active tab, selected cell,
selected sample, last-action timestamps). The renderer talks to the facade.

`Unit` owns the facade as a `unique_ptr`, created when the module exists:

```cpp
if (unit_type == "Extraction")
{
    prospectingSystem = std::make_unique<ProspectingSystem>(tier, gx, gy, resourceManager);
}
```

### Why the facade holds UI state

The renderer is immediate-mode and keeps nothing between frames. Anything
that must persist — which tab is open, which sample is selected — lives on
the facade. This also means the preview and playtest harnesses can drive a
module into any state without touching the renderer.

---

## 2. Declare units at subsystem boundaries

**The most expensive bug in this codebase so far**, and the rule that
prevents it recurring.

`ResourceManager` stores **absolute quantities** (hundreds to thousands per
cell). The prospecting chain assumed **composition fractions** (0–1). Nothing
in the type system objected: both are `float` in a
`std::map<ResourceType, float>`. The result was richness pinned at 100% for
every sample and compositions rendering as `-36104%`.

The rule:

> When a value crosses a subsystem boundary, **normalize at the boundary and
> name the unit in the API**.

`ProspectingGrid` is that boundary, and its API now says which is which:

```cpp
// Composition fractions (0-1, sum to ~1) -- "what this is made of"
std::map<ResourceType, float> GetGroundTruth(int subX, int subY, DepthLayer d) const;

// Absolute deposit quantity -- "how much is there"
float GetQuantity(int subX, int subY, DepthLayer d) const;
```

Consumers then pick deliberately: GPR sweep signal uses **quantity** (using
fractions would flatten the heat map, since fractions sum to 1 everywhere);
sample richness uses **quantity**; displayed composition uses **fractions**.

This distinction will recur in every unit — crop yield vs nutrient mix,
battery charge vs capacity, part mass vs material ratio. Name it every time.

**Related**: any constant that normalizes a real quantity must be calibrated
against real data, and should say so:

```cpp
// ResourceManager produces roughly 5,000 for an average cell layer and
// ~13,000 for a cluster core, before sub-cell variation (0.3x-2.0x).
constexpr float RICHNESS_NORMALIZATION = 10000.0f;
```

---

## 3. Tier gating

Put tier capability in constant tables, not in branching logic:

```cpp
constexpr int   MAX_DEPTH_PER_TIER[]     = { 1, 2, 3, 4 };
constexpr int   MAX_SWEEP_BAND_PER_TIER[] = { 0, 1, 2, 3 };
constexpr float DRILL_ENERGY_COST[4][4]  = { ... };   // [tier][depth]
```

Engines then answer capability questions (`CanSweep`, `CanDrill`,
`CanApplyTool`), and the UI asks rather than deciding for itself. Tier
upgrades propagate through the facade:

```cpp
void ProspectingSystem::SetTier(int newTier)   // resizes grid, updates every engine
```

The debug path `Unit::DebugUpgradeModuleTier()` bypasses tech/resource costs
— that is what the preview and playtest tools use to reach high tiers
without a colony economy.

---

## 4. Keep the contract with the rest of the game narrow

Prospecting exports exactly two things to the extraction pipeline:

```cpp
float GetSurveyProgress() const;   // 0-1
bool  IsMarkedSite() const;
```

`ProcessExtraction()` consumes those and nothing else. A whole subsystem —
five engines, four depth layers, a lab pipeline — was rewritten from scratch
without touching the extraction formula.

**Define this contract before building the module**, and resist widening it.
The narrow interface is what makes a module replaceable.

Cache derived values behind the contract when they are expensive:

```cpp
mutable CellSurveyResult cachedResult;
mutable bool cacheValid = false;      // invalidated by any mutable accessor
```

---

## 5. Hero visuals

A module that asks the player to inspect something benefits from a "hero"
visual set, keyed by a small struct:

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

The struct's fields index directly into the sprite tree
(`family_X/shape/size_N_glow_M.png`). Colour is applied as a **runtime
tint**, so one file set serves every material — 400 sprites cover all
combinations, and adding an element costs no art.

Render through a **lazy texture cache** keyed by path, unloaded in
`RenderManager`'s destructor. Load only the variants actually shown.

> **raylib gotcha**: draw calls are batched. Unloading a texture immediately
> after `DrawTexturePro` deletes it before the batch flushes and draws
> garbage. Load everything for a frame first, unload after `EndTextureMode`.

Planned equivalents for other units are in
`docs/design/ui/sprite-manifest.md` (Manufacture's `PartVisual` is fully
specified).

---

## 6. Multi-stage workflows

When a module has sequential stages, give each stage a tab rather than one
crowded panel. Prospecting uses **Sweep → Samples → Lab**, mirroring its
real workflow: survey broadly, sample selectively, analyse deeply.

Each stage should:
- **produce** something the next stage consumes,
- **cost** energy (see the completeness guide),
- **contribute** to the module's exported contract value.

Prospecting's survey progress is a weighted blend so every stage matters:

```cpp
constexpr float SURVEY_SWEEP_WEIGHT   = 0.20f;
constexpr float SURVEY_SAMPLE_WEIGHT  = 0.50f;
constexpr float SURVEY_TESTING_WEIGHT = 0.30f;
```

If a stage contributes nothing measurable, players will skip it — and you
have built a decorative UI, not a mechanic.

---

## 7. Checklist for a new module

- [ ] Directory under `src/<Module>/` with constants / types / engines / facade
- [ ] Engines contain no rendering or input code
- [ ] Facade owns engines + UI state; `Unit` owns the facade
- [ ] Boundary APIs name their units (quantity vs fraction vs rate)
- [ ] Normalization constants calibrated against dumped real data
- [ ] Tier capability in constant tables, queried via `Can*()` methods
- [ ] Narrow, documented contract with the rest of the game
- [ ] Sources added to `COLONY_CORE_SOURCES` in `src/CMakeLists.txt`
- [ ] Panel built per `docs/guides/ui-panels.md`
- [ ] Verified against `docs/guides/feature-completeness.md`
