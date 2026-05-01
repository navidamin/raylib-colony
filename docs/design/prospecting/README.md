# Prospecting Module — Design Documents

> **Auto-context rule:** When working on prospecting-related code (`src/Unit/unit.cpp` prospecting methods, `src/Engine/rendermanager.cpp` DrawProspectingPanel, prospecting input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [prospecting-master-design.md](prospecting-master-design.md) | Pipeline stages, multi-scale control, mechanics integration, gaps inventory | DRAFT |
| 2 | [sampling-mechanics.md](sampling-mechanics.md) | Science-based technology review + Design 7 variants (7A-7E) | DRAFT |
| 3 | [depth-sampling-design.md](depth-sampling-design.md) | Depth layers, resource distribution by depth, tier gating, default allocation | DRAFT |
| 4 | [confidence-system.md](confidence-system.md) | Formal confidence metric: scale, composition, per-tool behavior | DRAFT |
| 5 | [resource-distribution-model.md](resource-distribution-model.md) | Pathfinder correlations, geological coherence, clue chaining, map init | STUB |
| 6 | [ai-default-mode.md](ai-default-mode.md) | AI delegation logic, default behavior, efficiency penalties, heuristics | DRAFT |
| 7 | [ui-layout.md](ui-layout.md) | Prospecting menu views, sample visualization, stage-based interaction | DRAFT |

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
| File | Relevant Code |
|------|--------------|
| `src/Unit/unit.h` | ScanResult, ScanProfile, CampaignEntry, ProspectingObjective structs (lines 28-65, 144-158) |
| `src/Unit/unit.cpp` | PerformLIBSScan (1792-2040), ProcessExtraction scanMultiplier (1314-1338), Calibration (2204-2239), Campaigns (2243-2307), Objectives (2311-2494) |
| `src/Engine/rendermanager.cpp` | DrawProspectingPanel (1856-2493) |
| `src/Engine/inputmanager.cpp` | Prospecting input handling |
| `src/game_constants.h` | SURVEY_UNSCANNED_EFFICIENCY, SURVEY_SCANNED_BONUS, SURVEY_MARKED_SITE_BONUS |

### Existing Documentation
| Document | Location | Relevance |
|----------|----------|-----------|
| Extraction Unit Design v2 | `Prospecting_Extraction_Mechanics.md` (root) | Legacy comprehensive design — prospecting section to be superseded by this redesign |
| Overall Roadmap | `ROADMAP_OVERALL.md` (root) | Phase tracking, prospecting redesign scheduling |
| Imminent Roadmap | `ROADMAP_IMMINENT.md` (root) | Current sprint tasks |

### Related Module Designs
| Module | Dependency | Status |
|--------|-----------|--------|
| Excavation | Receives `surveyProgress` + `markedSites` from prospecting | Not yet in design/ |
| Beneficiation | Downstream of excavation, no direct prospecting dependency | Not yet in design/ |
| Resource Manager | Provides ground truth resource data that prospecting reveals | Existing code in `src/ResourceManager/` |
| **Research** | Funds AI automation upgrades via SCIENCE tokens (Section 11b) | [`docs/design/research/`](../research/README.md) — STUB |
| **AI Automation** | Cross-cutting pattern for all unit AI trees (prospecting is first client) | [`docs/design/ai-automation/`](../ai-automation/README.md) — STUB |

## Key Design Constraint

Whatever the prospecting redesign produces, it must output:
- `surveyProgress` (0.0-1.0) per grid cell
- Optional `markedSites` list

These feed the existing extraction efficiency formula in `ProcessExtraction()`:
```
scanMultiplier = (0.35 + 0.65 * surveyProgress) + (0.15 if marked) * objectiveBonus
```
This formula and `ProcessExtraction()` remain unchanged.
