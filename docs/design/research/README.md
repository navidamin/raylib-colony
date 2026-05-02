# Research Module — Design Documents

> **Status:** STUB — capturing interface requirements imposed by other modules.
> This is not yet a full design. It documents what the Research system **must** support based on decisions already made elsewhere.

> **Auto-context rule:** When working on Research unit code (`src/Unit/unit.cpp` Research methods, ResearchManager, UnlockRegistry), read this README first.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | This README | Interface requirements, current implementation, open questions | STUB |

## Current Implementation

Research units exist in code with 5 stub modules:

| Module | Type | Function | Status |
|--------|------|----------|--------|
| Laboratory | `LABORATORY` | Produces 5.0 SCIENCE/tick (100/day) | Functional |
| Analysis | `ANALYSIS` | Data analysis and computation | Stub (no unique logic) |
| Simulation | `SIMULATION` | Numerical simulation systems | Stub (no unique logic) |
| Archive | `ARCHIVE` | Research data storage and retrieval | Stub (no unique logic) |
| Publication | `PUBLICATION` | Research output and knowledge sharing | Stub (no unique logic) |

**Source:** `src/Unit/unit.cpp` `InitializeResearchModules()` (line ~691)

SCIENCE tokens accumulate in Colony `strategicReserves[ResourceType::SCIENCE]`. The UnlockRegistry (`src/UnlockRegistry/unlock_registry.h`) is a flat `std::set<std::string>` — techs are either unlocked or not, with no cost/queue mechanism.

## Interface Requirements (From Other Modules)

These are commitments already made in other design docs that the Research system must honor.

### From Prospecting AI (docs/design/ai-automation/)

The prospecting design (Section 11b) defines 8 AI research projects that consume SCIENCE tokens:

| Requirement | Detail |
|-------------|--------|
| **Project queue** | Players must be able to browse and purchase research projects |
| **SCIENCE costs** | Projects cost 200-2,500 SCIENCE each (total tree: 6,400) |
| **Global effects** | AI unlocks apply to all sects colony-wide, not per-unit |
| **Prerequisites** | Linear chain (each project requires the previous one) |
| **Tier gates** | Some projects also require specific module tiers (T1, T2 tools) |
| **Budget share** | AI prospecting is ~15% of total research budget (~45,000 SCIENCE) |

See: [ai-automation/README.md](../ai-automation/README.md) for the full AI automation pattern.

### From Extraction Module Tiers (Phase 1.5)

The UnlockRegistry already gates module tier upgrades with 14 tech strings:

| Module | Tier 1 | Tier 2 | Tier 3 |
|--------|--------|--------|--------|
| Prospecting | Spectroscopy | Geophysics | SwarmAI |
| Excavation | MechanizedDrilling | HeavyEquipment | AutonomousFleet |
| Beneficiation | MagneticSeparation | ProcessingChain | RefineryComplex |
| Operations | ShiftScheduling | AIScheduling | (unnamed) |
| Directives | BasicDirectives | AdvancedDirectives | AIGovernance |

**Current behavior:** These are unlocked via debug key F5, not through research spending. The Research system must eventually provide a proper path to unlock them.

**Requirement:** Each tech unlock needs a SCIENCE cost and potentially prerequisites. The ~25% extraction tech budget (~11,000 SCIENCE) covers all 14 of these.

### From Research Budget Model

The prospecting design established a full-game research budget model:

| Category | Budget Share | ~Tokens (45K total) | Owner |
|----------|-------------|---------------------|-------|
| Extraction tech tree | ~25% | ~11,000 | Module tier unlocks (14 techs) |
| Other unit tech trees | ~25% | ~11,000 | Farming/Energy/Mfg/Research tiers |
| Prospecting AI | ~15% | ~6,400 | AI automation upgrades |
| Future AI systems | ~15% | ~6,500 | Other unit automation |
| Colony-wide research | ~20% | ~9,000 | Transport, efficiency, etc. |

This budget assumes 1-5 Research units over a ~200-300 day game. The Research module design must ensure production rates support this.

## Open Design Questions

These must be resolved when the Research module is fully designed:

| # | Question | Depends On | Priority |
|---|----------|-----------|----------|
| 1 | How does the player browse and queue research projects? (UI) | Phase 6 Research UI | HIGH |
| 2 | Should AI unlocks and tier techs share the same queue, or separate queues? | ResearchManager design | HIGH |
| 3 | What do the Analysis/Simulation/Archive/Publication modules actually do? | Module specialization pass | MEDIUM |
| 4 | Do Research unit modules have their own tier upgrades? What techs gate them? | Tech tree design | MEDIUM |
| 5 | Should multiple Research units speed up the same project, or work in parallel? | ResearchManager design | MEDIUM |
| 6 | Are there colony-wide research bonuses (e.g., +10% research speed)? | Colony tech tree | LOW |
| 7 | Should the UnlockRegistry evolve into a full ResearchManager, or remain separate? | Architecture decision | MEDIUM |

## Cross-References

### Source Code
| File | Relevant Code |
|------|--------------|
| `src/Unit/unit.cpp` | `InitializeResearchModules()` (~line 691), `ResearchPointsPerTick` param (line 292) |
| `src/UnlockRegistry/unlock_registry.h` | Current stub tech system (14 techs, flat set, debug F5 unlock) |
| `src/Colony/colony.h` | `strategicReserves[SCIENCE]`, `reserveCapacity[SCIENCE]` |
| `src/game_constants.h` | `INITIAL_UNIT_SCIENCE` (20.0f) |

### Related Design Docs
| Document | Location | Relationship |
|----------|----------|-------------|
| Prospecting Master Design | `docs/design/prospecting/prospecting-master-design.md` | Section 11b defines AI research costs — first client of research system |
| AI Automation Pattern | `docs/design/ai-automation/README.md` | Cross-cutting pattern for all unit AI trees |
| Overall Roadmap | `ROADMAP_OVERALL.md` | Phase 6: Research & Technology |

### Roadmap
| Phase | Relevance |
|-------|-----------|
| Phase 1.5 (Complete) | Created UnlockRegistry stub, 14 extraction techs |
| Phase 6 (Planned) | Full Research system implementation |
