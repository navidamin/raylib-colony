# Prospecting Module — Design Documents

> **Auto-context rule:** When working on prospecting-related code (`src/Unit/unit.cpp` prospecting methods, `src/Engine/rendermanager.cpp` DrawProspectingPanel, prospecting input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [prospecting-master-design.md](prospecting-master-design.md) | Pipeline stages, multi-scale control, mechanics integration, gaps inventory | IMPLEMENTED — **panel and interaction superseded by #9** |
| 2 | [sampling-mechanics.md](sampling-mechanics.md) | Science-based technology review + Design 7 variants (7A-7E) | IMPLEMENTED |
| 3 | [depth-sampling-design.md](depth-sampling-design.md) | Depth layers, resource distribution by depth, tier gating, default allocation | IMPLEMENTED |
| 4 | [confidence-system.md](confidence-system.md) | Formal confidence metric: scale, composition, per-tool behavior | IMPLEMENTED |
| 5 | [resource-distribution-model.md](resource-distribution-model.md) | Pathfinder correlations, geological coherence, clue chaining, map init | STUB — undesigned, blocks pathfinder tips |
| 6 | [ai-default-mode.md](ai-default-mode.md) | AI delegation logic, default behavior, efficiency penalties, heuristics | DESIGNED, NOT BUILT (Phase 7) |
| 7 | [ui-layout.md](ui-layout.md) | Prospecting menu views, sample visualization, stage-based interaction | IMPLEMENTED — **superseded by #9** |
| 8 | [implementation-plan.md](implementation-plan.md) | **Resource classification.** Measured / Indicated / Inferred as a grouping of the existing confidence bands, on a colour key shared with excavation | C1-C4 BUILT |
| 9 | [block-model-design.md](block-model-design.md) | **THE NEW PROSPECTING.** Drill holes with azimuth and dip, an interpolated estimate field, and a four-layer isometric block model. Supersedes the panel and interaction design in #1 and #7 | PARTLY BUILT — view, one-screen panel, LIBS-only sweep, certain cores shipped; estimate field and line holes not |
| 10 | [progression-design.md](progression-design.md) | **PROGRESSION.** No tiers: rigs you buy (a stable, not a ladder) and techniques you learn (UnlockRegistry). Depth and reach ungated; disposition table for every `*_PER_TIER` constant | DESIGN — plan of record |
| 11 | [prototypes/drill-dock.html](prototypes/drill-dock.html) | **DRILL DOCK.** Layout study docking the hands-on borehole view against the 4-layer block model: one ground across both panels, one trace in two projections, three placements (`?v=a/b/c`). Graphics per [Dark Plating](../graphics/dark-plating.md) | PROTOTYPE |

> **Two designs live here.** #1-#8 describe **v1** as it was built. The game
> has since moved: the panel is one screen on a four-layer block model, the
> lab and GPR are gone, cores come out assayed, and a sweep can never
> classify. #9 is the design of record for the loop, #10 for progression.
> Start at #9 and #10 for new work; read #1-#8 for instrument details, with
> the caveat that anything about tabs, the lab, GPR bands, tier-gated depth
> or the reach ring is superseded.

## Design Summary

The prospecting module uses a **Core Samples** approach (Design 7) enhanced with science-based mechanics:

```
[Radar Sweep] → [Physical Sampling] → [Sample Tray] → [Multi-Tool Testing]
   (GPR)          (Core Drilling)      (Inventory)      (XRF/LIBS/Assay)
                                                              │
                                                     [Pathfinder Tips]
                                                     [Stratigraphy View]
```

**Multi-scale control:** Every stage has a default/auto mode (lower efficiency, lower confidence) and a fine-control mode (player-optimized). The player chooses engagement depth per stage.

## Cross-References

### Source Code (current implementation)

> The pre-2026-05 implementation (`PerformLIBSScan`, scan profiles, campaigns,
> objectives, `ProspectingAI` in `unit.cpp`) was **deleted** in the rewrite.

| File | Relevant Code |
|------|--------------|
| `src/Prospecting/prospecting_system.h/.cpp` | `ProspectingSystem` facade — owns engines + UI state, exports `GetSurveyProgress()` / `IsMarkedSite()` |
| `src/Prospecting/prospecting_grid.h/.cpp` | Sub-cell grid; `GetGroundTruth()` (composition fractions) vs `GetQuantity()` (absolute) |
| `src/Prospecting/sweep_engine.h/.cpp` | GPR bands, signal, noise, calibration |
| `src/Prospecting/sampling_engine.h/.cpp` | Drilling, sample creation, crystal visual assignment |
| `src/Prospecting/lab_engine.h/.cpp` | Analysis tools, separations, preset pipelines |
| `src/Prospecting/survey_progress_engine.h/.cpp` | Sweep/sample/testing → `surveyProgress` |
| `src/Prospecting/sample_tray.h/.cpp` | Tray capacity and sample lifecycle |
| `src/Prospecting/prospecting_constants.h` | Tier tables, energy costs, survey weights |
| `src/Engine/rendermanager.cpp` | `DrawProspectingPanel` (3 tabs), `DrawCrystalSprite`, energy gating helpers |
| `src/Unit/unit.cpp` | `ProcessExtraction` consumes survey progress |

### Existing Documentation
| Document | Location | Relevance |
|----------|----------|-----------|
| Extraction Unit Design v2 | `Prospecting_Extraction_Mechanics.md` (root) | Legacy comprehensive design — prospecting section to be superseded by this redesign |
| Overall Roadmap | `ROADMAP_OVERALL.md` (root) | Phase tracking, prospecting redesign scheduling |
| Imminent Roadmap | `ROADMAP_IMMINENT.md` (root) | Current sprint tasks |

### Related Module Designs
| Module | Dependency | Status |
|--------|-----------|--------|
| **Excavation** | Receives `surveyProgress` + `markedSites` from prospecting; proposed Design B also consumes per-sub-cell confidence for grade control | [`docs/design/excavation/`](../excavation/README.md) — DRAFT |
| Beneficiation | Downstream of excavation, no direct prospecting dependency | Not yet in design/ |
| Resource Manager | Provides ground truth resource data that prospecting reveals | Existing code in `src/ResourceManager/` |
| **Research** | Funds AI automation upgrades via SCIENCE tokens (Section 11b) | [`docs/design/research/`](../research/README.md) — STUB |
| **AI Automation** | Cross-cutting pattern for all unit AI trees (prospecting is first client) | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

## Implementation Status

| Phase | Status |
|---|---|
| 1. Data model & grid structure | ✅ Implemented |
| 2. Sweep mechanics (GPR) | ✅ Implemented |
| 3. Sampling mechanics | ✅ Implemented (crystal sprites rendered 2026-08) |
| 4. Lab / testing pipeline | ✅ Implemented (presets yes; custom drag-order pipeline no) |
| 5. Survey progress integration | ✅ Implemented (composition scale bug fixed 2026-08) |
| 6. UI rendering | ✅ Implemented (3 tabs; **no stratigraphy panel**) |
| 7. AI / default mode | ❌ Not started |
| 8. Objectives system | ❌ Not started |

Also outstanding: pathfinder tips / clue chaining (needs
[resource-distribution-model.md](resource-distribution-model.md) designed
first), and an energy-cost balance pass now that costs are enforced.

## Implementation Order

Build in this order — each phase is testable independently and builds on the previous one.

### Phase 1: Data Model & Grid Structure
- Sub-cell grid data (5×5 per extraction unit cell)
- `Sample` struct (depth layer, element composition, confidence, crystal visual properties)
- Sample tray (inventory container, 8-slot limit)
- Depth layer definitions (Regolith → Megaregolith → Fractured Bedrock → Intact Bedrock)
- Wire up to existing `ResourceManager` ground truth data

### Phase 2: Sweep Mechanics (GPR)
- Frequency slider → depth/resolution tradeoff calculation
- GPR sweep execution: energy cost, anomaly detection, confidence generation
- Heat map data generation per sub-cell (signal strength values)
- Calibration drift system (accuracy decay over sweeps)
- Basic heat map rendering on grid (neon thermal palette)

### Phase 3: Sampling Mechanics
- Drill-to-depth logic (energy cost, time per depth layer)
- Sample collection → populates tray with element data from ground truth + noise
- Tray management (collect, discard, reorder)
- Crystal visual property assignment (shape from depth family, color from element, glow from confidence, size from richness)

### Phase 4: Lab / Testing Pipeline
- Tool definitions (XRF, LIBS, Optical Microscopy, Fire Assay, Magnetic Susceptibility)
- Sequential tool application to samples (each reveals partial data, reduces uncertainty)
- Preset pipelines (Structural, Life Support, Rare Elements, Quick Survey)
- Custom pipeline building (drag-and-drop tool ordering)
- Fire assay as destructive chain-ender

### Phase 5: Survey Progress Integration
- Aggregate cell confidence from sweep data + analyzed samples
- Map confidence to `surveyProgress` (0.0–1.0) for extraction formula
- Marked sites from high-confidence + pathfinder tip cells
- Verify extraction efficiency responds correctly to new prospecting data

### Phase 6: UI Rendering
- Grid cell rendering (dark theme, states: unswept/swept/sampled/hover/selected/anomaly)
- Heat map overlay (40% opacity, brighten on hover)
- Crystal sprite rendering in sample tray and grid markers
- Stratigraphy side panel (vertical core column, correlation lines)
- Ghost buttons (sweep, collect, discard, presets, calibrate)
- Message bar (accent bar + log style, type-specific colors and fade)
- Tab navigation (Sweep → Samples → Lab)
- Objectives panel (bottom collapsible)

### Phase 7: AI / Default Mode
- Default sweep strategy (auto-frequency selection, grid coverage pattern)
- Default sampling heuristic (sample highest-signal cells, balanced depth)
- Default lab pipeline (Quick Survey preset)
- Efficiency penalty for AI mode vs player-optimized
- Toggle between manual and AI control per stage

### Phase 8: Objectives System
- Objective data model (type, target, progress, reward)
- Objective generation based on sweep/sample results
- Progress tracking and completion detection
- Bonus multiplier integration with extraction formula
- Objectives panel UI (collapsed list with progress bars)

### Dependencies Between Phases
```
Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4 ──► Phase 5
                                                    │
Phase 6 can start after Phase 3 (needs data to render)
Phase 7 can start after Phase 5 (needs full pipeline)
Phase 8 can start after Phase 5 (needs survey progress)
```

## Key Design Constraint

Whatever the prospecting redesign produces, it must output:
- `surveyProgress` (0.0-1.0) per grid cell
- Optional `markedSites` list

These feed the existing extraction efficiency formula in `ProcessExtraction()`:
```
scanMultiplier = (0.35 + 0.65 * surveyProgress) + (0.15 if marked) * objectiveBonus
```
This formula and `ProcessExtraction()` remain unchanged.
