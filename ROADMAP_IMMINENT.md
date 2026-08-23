# ROADMAP_IMMINENT.md

**Last Updated:** 2026-08-18
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

PHASE 1.5: Extraction Unit Overhaul ████████████████████ 100% ✅ COMPLETE
├─ Phases A-H (survey data, site selection, modules, pipeline) ✅ COMPLETE
├─ Module-specific UI rendering ✅ COMPLETE (display + interactive controls)
├─ Extraction UI redesign (dark sci-fi kit) ✅ COMPLETE (2026-08)
└─ Balance pass ✅ MOSTLY COMPLETE (upgrade costs still untuned)

PROSPECTING REWRITE ████████████████░░░░ ~75% (design Phases 1-6 of 8)
├─ Phase 1: Data model & sub-cell grid ✅ COMPLETE
├─ Phase 2: Sweep mechanics (GPR) ✅ COMPLETE
├─ Phase 3: Sampling & crystal visuals ✅ COMPLETE (sprites rendered 2026-08)
├─ Phase 4: Lab pipeline ✅ COMPLETE (tools, separations, presets)
├─ Phase 5: Survey progress aggregation ✅ COMPLETE (scale bug fixed 2026-08)
├─ Phase 6: UI rendering ✅ COMPLETE (3 tabs, all controls reachable)
├─ Phase 7: AI / default mode ❌ NOT STARTED
└─ Phase 8: Objectives system ❌ NOT STARTED

PHASE 1: Core Resource System ██████░░░░░░░░░░░░░░ ~30% NEXT (1.2/1.3 partially addressed)
PHASE 2: Transport Network █████████████████░░░ ~90% LARGELY COMPLETE
PHASE 3: Advanced Production ░░░░░░░░░░░░░░░░░░░░  0% PLANNED
...
```

---

## Recent Completions (2026-08-18)

### Sect View Visual Redesign ✅ COMPLETE

- **Orbital layout** - Sect view rebuilt as a hub-and-spoke base: central
  hex-glass dome (development readout), 8 unit dome stations, connector arms
  with status conduits and socket LEDs, outer ring road with crossbar lamp
  seams, twin entry rails with gate boxes. Fully procedural (no sprite assets).
- **Ray-shaded domes** - Per-pixel sphere lighting (Lambert + two-lobe Blinn
  specular + fresnel + bounce) baked into cached textures per tint/size/seed;
  per-unit lighting character seeded from unit type name.
- **Procedural unit glyphs** - 8 icon drawings (derrick, sprout, factory,
  truck, tower, flask, bolt, crane) replacing texture thumbnails in this view.
- **Design docs** - New `docs/design/sect-view/` module (HUD element
  inventory, surroundings brainstorm, hover tooltip design) registered in the
  design index and CLAUDE.md auto-context table.
- Follow-up work is planned in `docs/design/sect-view/sect-view-elements.md`
  (tooltip card frame, blocker detection, alert badges).

## Recent Completions (2026-08)

### Prospecting Polish, Economy & Dev Tooling ✅ COMPLETE

Built on the May 2026 prospecting rewrite (see next section).

**Gameplay / correctness**
- **Composition scale fix** — `ResourceManager` stores absolute quantities
  (hundreds–thousands); the prospecting chain assumed 0-1 fractions.
  Richness was pinned at 100% and compositions rendered as `-36104%`.
  `ProspectingGrid` now separates `GetGroundTruth()` (composition fractions)
  from `GetQuantity()` (absolute quantity); sweep signal uses quantity,
  displayed composition uses fractions. `RICHNESS_NORMALIZATION` recalibrated
  2.0 → 10000.0 against dumped real data.
- **Three unreachable features wired up** — CALIBRATE (calibration decayed
  with no restore path), DISCARD SAMPLE (a full 16/16 tray permanently
  blocked collecting), and lab PRESETS (`ApplyPreset` had no UI at all).
- **Energy costs enforced** — sweeps/drills/tools/separations/presets are now
  charged against unit storage, gated before commit (unaffordable controls
  grey out), refunded on failed actions, with stored energy shown in the
  status bar (gold <300 E, red <100 E).
- **Crystal sprites rendered** — 400 pre-rendered sample sprites were
  committed but never drawn; the tray showed letters. Now drawn via a lazy
  texture cache with runtime element tinting.

**UI**
- Full extraction-unit redesign to the dark sci-fi kit: design tokens,
  procedural line icons, floating cards, hazard-striped destructive buttons,
  segmented gauges, wireframe blueprint art.
- Theme applied to all extraction menus (sweep/samples/lab, excavation,
  beneficiation, operations, directives, overview).
- Touch-first feedback (pressed / flash / persistent applied state) and
  radio-style selection rows.

**Tooling & docs** (see `docs/dev-workflow.md`)
- `tools/preview/` — headless panel screenshots (~5s for a 12-panel set),
  fixed world seed for reproducibility
- `tools/playtest/` — interactive prospecting sandbox, also built for Web and
  deployed to GitHub Pages `/playtest/` for phone testing
- `tools/inspect/` — dumps real generated data; found the composition bug
- `tools/shell-test/` — canvas-fit regression test for `minshell.html`
- Mobile web fix (SHELL v4): framebuffer pinning + canvas fitting
- Guides: `docs/guides/ui-panels.md`, `module-architecture.md`,
  `feature-completeness.md`

### Prospecting Rewrite ✅ COMPLETE (2026-05)

The prospecting system was **rebuilt from scratch** into `src/Prospecting/`
following `docs/design/prospecting/`. A Core Samples model replaced the
previous scan-based system.

- New subsystem: `ProspectingGrid`, `SweepEngine`, `SamplingEngine`,
  `LabEngine`, `SurveyProgressEngine`, `SampleTray`, fronted by the
  `ProspectingSystem` facade
- Sweep (GPR bands) → Sample (core drilling, 4 depth layers) → Lab
  (XRF/LIBS/assay/separations) → survey progress
- Crystal sample sprite set generated (`tools/crystal_gen`, 400 sprites)
- Contract with extraction unchanged: `surveyProgress` + `markedSites`

> **⚠️ The sections below describe the pre-rewrite prospecting system.**
> `PerformLIBSScan`, scan profiles, calibration standards, adaptive infill
> campaigns, prospecting objectives, and `ProspectingAI` were **removed** in
> the rewrite. Objectives and AI are planned to return as design Phases 8
> and 7. Retained for historical context only.

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

### Resource Model Split (2026-02-08) ✅ COMPLETE

- **ResourceDescriptor table** - Single source of truth (`resource_types.h`) for name, color, category (SINGULAR/TYPED), and subtypes per resource
- **Typed resource flow** - `Colony::ReceiveTypedSurplus()` / `Colony::ProvideTypedResource()` wired into Sect push/pull
- **Auto-balance iteration fix** - Colony deficit transport and auto-balance now iterate descriptors via `GetResourceDescriptors()` instead of raw `static_cast<int>` loops
- **New typed resources** - MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS added to enum with subtypes

### Extraction UI Font Scaling (2026-02-08) ✅ COMPLETE

- **Font texture size** - Increased from 32pt to 48pt for crisper rendering
- **FS() size multiplier** - All extraction view `DrawTextEx`/`MeasureTextEx` calls wrapped with `FS()` returning `baseSize * 1.30f` (XL preset)
- **Scan log improvements** - Prospecting scan history bars enlarged (10px → 16px), element labels now show name + percentage (e.g. "Fe 42%")
- **Font choice** - Evaluated Orbitron, Rajdhani, Chakra Petch, Titillium Web; kept Exo 2 (Regular + Bold)

### Prospecting Gameplay Overhaul (2026-02-21) ✅ COMPLETE

- **Scan-gated extraction** - `ProcessExtraction()` applies `scanMultiplier`: 0.35 (unscanned), 1.0 (scanned), 1.15 (marked). Prospecting is now mandatory for efficient extraction.
- **Tier-dependent scan noise** - `PerformLIBSScan()` rewritten:
  - Tier 0: Visual estimation — quality stars + LOW/MED/HIGH categories only, no numbers. 5s cooldown, 10 energy.
  - Tier 1: LIBS — numeric values with ±15% noise. 3s cooldown, 50 energy.
  - Tier 2: Multi-spectral — ±5% noise + minerals + hydrogen.
  - Tier 3: Deep survey — exact values, no noise.
- **Tier 0 scanning enabled** - Previously blocked at `tier >= 1`; now all tiers can scan. Tier 0 provides extraction rate unlock but no precise data.
- **Colony Ctrl overlay nerfed** - "RESOURCE PREVIEW" → "ORBITAL SURVEY". Removed exact abundance numbers. Shows only LOW/MED/HIGH categories with "(prospect for detail)" hint.
- **Geological confidence system** - `GetGeologicalConfidence()` counts scanned cells in 5x5 grid (each = 4%). Up to +10% bonus added to `GetOperationsEfficiencyModifier()` when Operations module is active.
- **UI updates** - Prospecting panel: tier-aware titles (VISUAL ESTIMATION / LIBS SCANNER / etc.), accuracy labels, confidence meter. Scan history: Tier 0 shows category tags, Tier 1+ shows composition bars. Operations panel: shows survey coverage bonus. Cooldown bar adjusts for 5s (T0) vs 3s (T1+).
- **Data model** - `ScanResult` extended with `scanTier` (int) and `categories` (map<ResourceType, string>).

### Prospecting Phase 2 Expansion (2026-02-21) ✅ COMPLETE

Six new mechanics adding depth to the prospecting system, plus AI auto-management:

- **Scan Profiles** - Three configurable presets (Quick/Standard/Deep) affecting power, pulse count, cooldown, energy cost, and survey progress multiplier (0.6×/1.0×/1.5×). T0 locked to "Visual" (0.8×); T1+ selects profiles.
- **Survey Progress Model** - Each cell tracks `surveyProgress` (0-100%). Each scan adds `baseTierProgress × profileSurveyMult × calibrationQuality × √(1 - currentSurvey)`. Diminishing returns, every scan helps. T0→~25 scans to 100%, T1→~8, T2→~6, T3→~3. Extraction efficiency = `0.35 + 0.65 × surveyProgress`. Replaces old scanCount/3 hard cap.
- **Calibration Drift & Standards** - `calibrationQuality` degrades 0.02 per scan (floor 0.5). Directly multiplies survey progress gain per scan. Manual calibration (30s, blocks scanning) restores to 1.0. T3 auto-calibration eliminates drift. Colored gauge in UI.
- **Depth Profiling** - `DepthLayer` enum (SURFACE/SHALLOW/MID/DEEP) with depth-biased resource generation. H2 concentrated on surface, Fe/Ti concentrated deep. Excavator depth determines which layer is extracted. T1 scans reveal surface, T2 adds shallow, T3 reveals all 4. Column visualization on hover.
- **Adaptive Infill Campaign** - Queue cells for automated sequential scanning (T2+, cap 10; T3 unlimited). START/PAUSE/CLEAR controls. Completed campaigns award +5% geological confidence. Middle-click to queue.
- **Prospecting Objectives** - Three objective types: THRESHOLD (find rich vein → +25% extraction for 5 days), COVERAGE (scan N cells → +5% permanent confidence), GRADIENT (find deposit edge → +15% for 3 days). Generated per tier (T1=1, T2=2, T3=3). Completed objectives replaced with new ones.
- **AI Auto-Management** - `ProspectingAI` struct with auto profile selection (Quick for first scan, Deep for marked, Standard for low quality), auto calibration (triggers when quality < threshold), and T3 auto campaign (spiral survey of unscanned cells). Toggle checkboxes in UI. AI decisions displayed as messages.

**Files modified:** `game_enums.h` (DepthLayer enum), `game_constants.h` (calibration/campaign/objective/survey constants), `resource_manager.h/.cpp` (LayeredResourceTile, depth-biased generation, GetResourcesAtGridLayer), `unit.h` (4 structs, extended ScanResult with surveyProgress, ScanProfile with surveyMultiplier, 20+ new methods), `unit.cpp` (all logic), `rendermanager.cpp` (full DrawProspectingPanel overhaul).

### Survey Progress Rework + Panel UI Fixes (2026-02-22) ✅ COMPLETE

Replaced opaque scanCount/3 extraction formula with transparent **Survey Progress** model, plus panel UI fixes:

- **Survey Progress Model** - Per-cell `surveyProgress` (0.0-1.0) replaces scanCount-based extraction gating. Formula: `progressGain = baseTierProgress × profileSurveyMult × calQuality × √(1 - currentSurvey)`. Every scan always helps (diminishing returns, no hard cap). Base progress: T0=10%, T1=20%, T2=30%, T3=45%. Extraction efficiency: `0.35 + 0.65 × surveyProgress`.
- **Profile survey multipliers** - Quick=0.6×, Standard=1.0×, Deep=1.5× directly multiply survey gain. Profile tooltip now shows "Survey: X.Xx" instead of "Noise: X.Xx".
- **Calibration affects survey gain** - `calibrationQuality` now directly multiplies survey progress per scan (not just hidden noise). Player sees: low calibration → less survey % per scan → time to recalibrate.
- **UI: Survey bars on grid cells** - Mini progress bar (3px, color-coded) at bottom of each scanned cell + percentage text, replacing old scan count badge.
- **UI: Survey + Efficiency readout** - After interaction hints: progress bar with "Survey: XX%", then "Extraction Efficiency: XX%".
- **UI: Deferred tooltip rendering** - Hover tooltip now draws AFTER depth profile (z-order fix).
- **UI: Depth profile limited to tier** - Only shows bands the current tier can scan (T1→1 band, T2→2, T3→all) + upgrade hint.
- **UI: AI management relocated** - Compact inline row near scan profile buttons instead of overflowing bottom section.
- **Backward compat** - Old scans with scanCount>0 but surveyProgress=0 are migrated using old formula.

**Files modified:** `game_constants.h` (7 SURVEY_* constants), `unit.h` (surveyProgress in ScanResult, surveyMultiplier in ScanProfile, GetSurveyProgress), `unit.cpp` (PerformLIBSScan survey calc, ProcessExtraction formula, AI logic, GetSurveyProgress), `rendermanager.cpp` (DrawProspectingPanel overhaul).

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

### Prospecting — Remaining Design Phases 📋 OPEN
**Priority:** MEDIUM
**Scope:** The two unbuilt phases of `docs/design/prospecting/README.md`

- [ ] **Phase 8: Objectives** — data model, generation from sweep/sample
      results, progress tracking, reward multipliers into extraction
- [ ] **Phase 7: AI / default mode** — auto-sweep/sample/lab heuristics,
      per-stage manual↔AI toggle, efficiency penalty. Design says this needs
      the full pipeline working first (it now is). See
      `docs/design/ai-automation/README.md` for the cross-cutting pattern.
- [ ] Pathfinder tips / clue chaining — `resource-distribution-model.md` is
      still a STUB and needs design before code
- [ ] Stratigraphy side panel (core column + correlation lines)
- [ ] Custom lab pipeline builder (presets exist; drag-to-order does not)
- [ ] Energy cost balance pass — a full six-tool workup on a 16-slot tray
      costs ~2,960 E against a 1,000 E starting reserve. Deliberate tension,
      but unplayed; needs a real session to judge.

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
4. **Debug key (F5)** - Should be removed or gated behind debug build flag before release.
5. **Message fade path split** - For extraction units, UpdateMessage is called in Unit::Update rather than DrawInUnitView. This works but is a different code path than non-extraction units.
6. **Non-extraction units use the legacy UI** - Farming/Energy/Manufacture/Research still render through `unit->DrawInUnitView()` (`unit_ui.cpp`), not `RenderManager`, so they get none of the themed chrome or preview-tool support. Route each through `RenderManager` when its real panel is built (see `docs/guides/ui-panels.md`).
7. **Four unit types are stubs** - Farming, Energy, Manufacture, and Research each have five *named* modules but only generic production logic. This is the largest content gap in the game.
8. **Prospecting design docs still marked DRAFT/STUB** - Phases 1-6 are implemented; the docs do not say so. `resource-distribution-model.md` is a genuine STUB (pathfinder tips undesigned).
9. **Roadmaps drifted ~6 months** - This file and `ROADMAP_OVERALL.md` described the pre-rewrite prospecting system until 2026-08-13. Update them at the end of each session per the `CLAUDE.md` catchup procedure.

---

## Next Sprint Preview

The prospecting loop is now playable end-to-end (real data, real costs, no
dead ends) and testable on desktop, headlessly, and on phone. Candidate next
moves, roughly in order of value:

1. **Playtest prospecting on device** — the loop has never had a real session
   with working data and enforced costs. Do this before tuning anything.
2. **Prospecting Phase 8: Objectives** — gives the loop goals and reward
   multipliers; smaller than Phase 7 and unblocks it.
3. **Prospecting Phase 7: AI / default mode** — needed before the player owns
   many sects (see the ×20 scale question in
   `docs/guides/module-architecture.md`).
4. **First non-extraction unit panel** — Farming or Energy, built to
   `docs/guides/module-architecture.md` and `ui-panels.md`. This is the
   biggest content gap and the best test of whether the guides generalize.
5. **Phase 1: Core Resource System** — storage capacity (1.1), resource
   visualization (1.2), transport timing (1.3), consumption/distribution (1.4)
6. **Save/load** — increasingly painful as systems accumulate

### Future Prospecting Ideas (not scheduled)
- Global AI Manager that sets policies across all extraction units colony-wide
- Resource Contour Map (heatmap interpolation)
- Spectral Interference & Peak Resolution — adds challenge once core systems are solid
- Subsurface Anomaly Detection (GPR structures: lava tubes, ice deposits)
- Core Drill Sampling — definitive depth data via excavator commitment
- Sample Collection & Assay Queue — timed lab analysis as alternative to field LIBS
