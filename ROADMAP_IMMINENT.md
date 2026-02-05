# ROADMAP_IMMINENT.md

**Last Updated:** 2026-02-01
**Current Sprint:** Prospecting & Extraction Unit Overhaul
**Timeline:** Phase 1.5 - Extraction Unit Overhaul

---

## Where We Stand in Overall Roadmap

```
PHASE 0: Foundation & Architecture ███████████████████░ 85% MOSTLY COMPLETE
├─ Engine refactor ✅ COMPLETE
├─ View system ✅ COMPLETE
├─ Entity hierarchy ✅ COMPLETE
├─ Module system ✅ COMPLETE
├─ Data-driven architecture ✅ COMPLETE (Checkpoint 1)
├─ Graphics enhancement ~95% COMPLETE (needs polish pass)
└─ Transport network ✅ COMPLETE (~90%)

PHASE 1.5: Extraction Unit Overhaul ████████████████████ ~99% ⚡ CURRENT
├─ Phase A: Foundation (Survey Data & Unlock Registry) ✅ COMPLETE
├─ Phase B: Site Selection UI ✅ COMPLETE
├─ Phase C: Module Architecture Overhaul ✅ COMPLETE
├─ Phase D: Prospecting Module (Tiers 0-3) ✅ COMPLETE (data model)
├─ Phase E: Excavation Module (Tiers 0-3) ✅ COMPLETE (data model)
├─ Phase F: Beneficiation Module (Tiers 0-3) ✅ COMPLETE (data model)
├─ Phase G: Operations & Directives ✅ COMPLETE (data model + handlers)
├─ Phase H: Integration & Pipeline ✅ COMPLETE
├─ Module-specific UI rendering ✅ COMPLETE (display + interactive controls)
└─ Balance pass ✅ MOSTLY COMPLETE (bug fix + efficiency rebalance)

PHASE 1: Core Resource System ░░░░░░░░░░░░░░░░░░░░  0% NEXT
PHASE 2: Transport Network █████████████████░░░ ~90% LARGELY COMPLETE
PHASE 3: Advanced Production ░░░░░░░░░░░░░░░░░░░░  0% PLANNED
...
```

---

## Recent Completions (2026-01-31)

### Phase A: Foundation - Survey Data & Unlock Registry ✅ COMPLETE

- **A1: Orbital Survey Data Model** - Added `OrbitalSurveyData` struct to ResourceManager with 11 fields (Fe%, Ti%, Si%, Al%, Ca%, Th ppm, K ppm, hydrogen signal, solar illumination, terrain slope, earth visibility). Procedural generation from resource clusters. `GetSiteArchetype()` classification.
- **A2: Unlock Registry** - Created header-only `UnlockRegistry` singleton (`src/UnlockRegistry/unlock_registry.h`) with 14 available techs. Debug key F5 cycles unlocks, F6 prints survey data.
- **A3: Missing Resource Types** - Added Ti, Al, Ca to `ResourceType` enum, resource generation, game_types.toml.

### Phase B: Site Selection UI ✅ COMPLETE

- **B1: Colony Site Selection View** - Added `View::SITE_SELECTION` enum. Ctrl+click in Planet view enters site selection mode instead of placing colony immediately.
- **B2: Instrument Panels** - Full `DrawSiteSelectionView()` with GRS bar charts, Neutron Spectrometer, Thermal Mapper, Site Assessment panel with archetype recommendation.
- **B3: Confirm Site & Archetype** - Colony receives `SiteArchetype` with bonus multipliers (Mare +20% Fe/Ti, Highland +20% Si/Al, Polar +50% H2/WATER, KREEP +30% Science, Lava Tube +15% all).
- **B4: Sect Placement Preview** - Ctrl+hover in Colony view shows resource tooltip (Fe, Ti, Si, Al, Ca, H2 with HIGH/MED/LOW ratings).

### Phase C: Module Architecture Overhaul ✅ COMPLETE

- **C1-C2: Extraction Modules** - 5 specialized modules: PROSPECTING, EXCAVATION, BENEFICIATION, OPERATIONS, DIRECTIVES. All tier 0-3 with dependencies.
- **C3: Stub Modules for Other Units** - Farming (5), Energy (5), Manufacture (5), Research (5) modules named and structured.
- **C4: game_types.toml** - Full tier data for all 5 extraction modules with energy, dependencies, consumption, upgrade costs.
- **C5: Tier Upgrade Logic** - `UpgradeModuleTier()` checks UnlockRegistry and resource costs.

### Phase D: Prospecting Module ✅ COMPLETE (Data Model)

- `ScanResult` struct with elements, minerals, hydrogen, quality
- `PerformLIBSScan()` with tier gating, cooldown, energy cost
- `MarkSiteForExcavation()` / `UnmarkSite()`
- Scan cooldown tracking in Update()

### Phase E: Excavation Module ✅ COMPLETE (Data Model)

- `Excavator` struct (id, gridPos, method, depth, rate, wear)
- `MoveExcavator()`, `SetExcavatorDepth()`, `SetExcavatorRate()`
- Tier-based max depth and excavator counts
- Wear accumulation in Update()

### Phase F: Beneficiation Module ✅ COMPLETE (Data Model)

- `SeparationNode` struct with 7 types (SIZE_SORT, MAGNETIC, ELECTROSTATIC, THERMAL, CHEMICAL, MRE, DIRECT_OUTPUT)
- Predefined factory functions in `SeparationNodes` namespace
- Chain management: `SwapSeparationNodes`, `AddSeparationNode`, `RemoveSeparationNode`
- Tier upgrade rebuilds separation chain

### Phase G: Operations & Directives ✅ COMPLETE (Data Model)

- `DirectiveType` enum: NONE, PRIORITIZE, MAXIMIZE, CONSERVE, EXPLORATION_MODE, EMERGENCY_HARVEST, THERMAL_SYNC
- `ActiveDirective` struct with type, target resource, strength
- `SetDirective()` with tier gating
- `GetOperationsEfficiencyModifier()`: tier 0=0.85, tier 1=1.0, tier 2=1.1, tier 3=1.20

### Phase H: Integration ✅ COMPLETE

- Rewrote `ProcessExtraction()` with full pipeline:
  - Stage 1: Excavation (operations modifier, directive modifier, excavator count)
  - Stage 2: Beneficiation (separation chain processing)
  - Stage 3: Add to storage
- Updated ROADMAP_OVERALL.md with Phase 1.5 section

### Extraction Unit UI Overhaul ✅ COMPLETE (Display Panels)

- Dark-themed RenderManager-based UI for extraction units (non-extraction units fall back to old white UI)
- **Left panel:** Module list with hover/selection highlighting, status borders (green=active, gray=inactive, red=locked)
- **Center panel:** 5 module-specific display panels:
  - Prospecting: scan grid visualization, scan history, marked sites
  - Excavation: excavator fleet table, depth/rate/wear display
  - Beneficiation: separation chain node visualization with efficiency/wear
  - Operations: efficiency modifier display, shift status
  - Directives: active directive display, available directives list
- **Right panel:** Build/Upgrade/Activate/Deactivate buttons with cost breakdown, tier info
- **Resource overview:** Production/consumption table and storage bars
- **Bottom bar:** Message fade system (UpdateMessage called in Unit::Update)

---

## Remaining Tasks

### Module-Specific Interactive Controls ✅ COMPLETE (2026-02-01)
**Priority:** HIGH
**Scope:** Add interactive controls to extraction module panels (display panels already complete)

#### Tasks
- [x] Prospecting: clickable 5x5 scan grid, left-click scans, right-click marks/unmarks sites, cooldown overlay, tier gate overlay
- [x] Excavation: [-] value [+] buttons for depth (tier-dependent step) and rate (5 kg/hr step) per excavator
- [x] Beneficiation: up/down arrow buttons to reorder chain, clickable ON/OFF toggle per node
- [x] Directives: clickable directive selection cards with tier gating, PRIORITIZE resource chip selector

### Balance Pass ✅ MOSTLY COMPLETE (2026-02-01)
**Priority:** MEDIUM
**Scope:** Tune extraction rates, energy costs, upgrade costs per tier

#### Tasks
- [x] Dynamic energy consumption scaling (Excavation by excavator count, Beneficiation by chain length, Operations by tier, Directives by active directive)
- [x] Remove `energyRequired` dead code from `UpgradeModuleTier()`
- [x] Fixed beneficiation double-multiply bug (efficiency was applied per-node, compounding N times)
- [x] Raised separation node efficiencies (SIZE_SORT 0.92, MAGNETIC 0.88, ELECTROSTATIC 0.85, THERMAL 0.82, MRE 0.90)
- [x] Added missing directive handlers (EMERGENCY_HARVEST +50%/+wear, EXPLORATION_MODE -50%, THERMAL_SYNC sinusoidal day-cycle)
- [x] Bumped Operations Tier 3 modifier from 1.15 to 1.20
- [ ] Tune upgrade costs per tier (resource amounts)
- [ ] Test full pipeline throughput at each tier (manual playtesting)

### Graphics Polish (Checkpoint 0 Remainder) 📋 DEFERRED
**Priority:** LOW
**Scope:** Texture scaling refinement, active/inactive tint, remaining visual polish

---

## Previous Completions

### Checkpoint 1: Data-Driven Type System ✅ COMPLETE (2025-11-09)
- tomlplusplus v3.4.0 integrated via CPM
- GameTypesLoader singleton class
- game_types.toml with resources, unit types, modules

### Checkpoint 0: Graphics Enhancement ~95% COMPLETE
- Texture-based rendering for dome and units in Sect view
- Needs polish pass (texture scaling, active/inactive tint refinement)

### Transport Network ✅ COMPLETE (~90%)
- Road construction mode with UI feedback
- Transport rate limiting and multiple packets
- Visual feedback for selected roads

---

## Known Issues & Technical Debt

1. **InitializeFutureModules() dead code** - Old function still exists but no longer called for Extraction units. Should be removed.
2. **No save/load system** - Game state lost on exit.
3. ~~**Module interactive controls not yet implemented**~~ ✅ RESOLVED 2026-02-01 - All four panels now interactive.
4. **Debug keys (F5/F6)** - Should be removed or gated behind debug build flag before release.
5. **Message fade path split** - For extraction units, UpdateMessage is called in Unit::Update rather than DrawInUnitView. This works but is a different code path than non-extraction units.

---

## Next Sprint Preview

After completing remaining tasks, next priorities:
1. **Tune upgrade costs** - Adjust resource costs per tier for engaging progression
2. **Manual playtesting** - Full pipeline throughput verification at each tier
3. **Phase 1: Core Resource System** - Resource flow, graceful degradation, allocator
4. **Phase 3: Advanced Production** - Manufacturing chains, research trees
