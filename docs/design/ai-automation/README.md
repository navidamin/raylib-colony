# AI Automation — Cross-Cutting Design Pattern

> **Status:** STUB — establishing the pattern from the prospecting AI tree as a template for all unit types.

> **Auto-context rule:** When designing AI/automation features for any unit type, read this doc first to ensure consistency.

## Overview

AI automation is a **cross-cutting system** that spans all unit types. Each unit type can have an AI research tree that lets the player invest SCIENCE tokens to automate that unit's gameplay loop. This document defines the shared pattern; individual module docs define the specific trees.

The pattern was established by the prospecting module (Section 11b of `docs/design/prospecting/prospecting-master-design.md`).

## Core Pattern

### Principles

1. **Research-gated, not progression-gated.** AI capabilities are purchased from the colony SCIENCE pool, not earned by playing the unit. This makes AI investment a strategic colony-management decision.

2. **Global effect.** Once purchased, an AI capability applies to **all sects** colony-wide. A player who invests in AI can scale to many sects without micromanaging each one.

3. **Tiered capability curve.** Each tree follows the same cost/capability arc:
   - **Free baseline** — basic automation available immediately, noticeably worse than manual play
   - **Cheap convenience** — quality-of-life (automate tedious steps), available within a few days
   - **Medium intelligence** — AI makes smart decisions using game data, available mid-game
   - **Expensive mastery** — AI approaches player-quality decisions, late-game capstone

4. **Manual always available.** AI never replaces manual control — it provides a Default/Auto mode that players can override at any time per-unit.

5. **Efficiency penalty at low tiers.** Unupgraded AI is noticeably worse than a skilled player. This gives the player a reason to engage manually early on, and a reason to invest in AI later.

### Cost Structure Template

Each AI tree should follow this approximate structure:

| Tier | Label | Cost Range | Research-Days (1 unit) | Purpose |
|------|-------|-----------|----------------------|---------|
| 0 | Baseline | Free | 0 | Basic automation, clearly suboptimal |
| 1a | Convenience A | 200 SCIENCE | 2 days | Remove a tedious manual step |
| 1b | Convenience B | 200 SCIENCE | 2 days | Remove another tedious step |
| 2a | Intelligence A | 500 SCIENCE | 5 days | Use game data for smarter decisions |
| 2b | Intelligence B | 500 SCIENCE | 5 days | Another smart-decision upgrade |
| 2c | Intelligence C | 500 SCIENCE | 5 days | Third smart-decision upgrade |
| 3a | Mastery A | 1,000 SCIENCE | 10 days | Near-player-quality in one area |
| 3b | Mastery B | 1,000 SCIENCE | 10 days | Near-player-quality in another area |
| 4 | Full Autonomy | 2,500 SCIENCE | 25 days | Fire-and-forget, all capabilities combined |

**Target total:** ~6,000-7,000 SCIENCE per unit type (~15% of full-game research budget).

Not every tree needs exactly 9 upgrades. The cost tiers (200/500/1000/2500) and the total budget target are the key constraints.

### Prerequisite Patterns

AI trees can use two types of prerequisites:

1. **Linear chain** — each upgrade requires the previous one (simplest, used by prospecting)
2. **Branching** — upgrades branch into specializations that converge at Full Autonomy

Both are valid. Linear is recommended unless the unit type has clearly distinct automation sub-problems.

Some upgrades may also require **module tier gates** (e.g., "requires T2 tools unlocked") in addition to the previous AI upgrade. This ties AI capability to the player's investment in the unit's physical infrastructure.

## Per-Module AI Trees

### Prospecting — DESIGNED

**Status:** Fully costed and designed in `docs/design/prospecting/prospecting-master-design.md` Section 11b.

| Project | Cost | Effect |
|---------|------|--------|
| Basic Automation | Free | Random drilling, visual inspection, stops when tray full |
| Auto-Collection | 200 | Collects samples without player input |
| Auto-Discard | 200 | Replaces lowest-value sample when tray full |
| Sweep-Guided Targeting | 500 | Uses sweep heat map instead of random targeting |
| Tool Matching | 500 | Selects optimal tool per element type |
| Multi-Tool Pipelines | 500 | Sequential multi-tool analysis |
| Context-Aware Presets | 1,000 | Adapts to site characteristics |
| Precision Calibration | 1,000 | Removes AI confidence penalty |
| Full Autonomy | 2,500 | Near-player-quality across all decisions |
| **Total** | **6,400** | |

### Excavation — NOT YET DESIGNED

Potential automation areas:
- Auto-depth selection based on survey data
- Fleet management (excavator allocation, wear rotation)
- Rate optimization based on storage capacity
- Deposit-tracking (follow rich veins as they deplete)

### Farming — NOT YET DESIGNED

Potential automation areas:
- Crop rotation scheduling
- Harvest timing optimization
- Water/nutrient balancing
- Yield prediction and surplus planning

### Energy — NOT YET DESIGNED

Potential automation areas:
- Load balancing across consumers
- Solar tracking / thermal cycle optimization
- Peak shaving and storage management
- Predictive generation scheduling

### Manufacturing — NOT YET DESIGNED

Potential automation areas:
- Recipe selection based on demand
- Input material substitution
- Batch size optimization
- Supply chain coordination

### Research — NOT YET DESIGNED

Potential automation areas:
- Auto-queue next priority project
- Cross-colony research coordination
- Publication timing (when to share vs. hoard results)

## Research Budget Context

AI automation trees across all unit types share a combined ~30% of the total research budget:

| Allocation | Tokens | Status |
|-----------|--------|--------|
| Prospecting AI | ~6,400 | Designed |
| Excavation AI | ~6,500 | Future |
| Farming AI | ~6,500 | Future |
| Energy AI | ~6,500 | Future |
| Manufacturing AI | ~6,500 | Future |
| Research AI | ~3,000 (smaller scope) | Future |

**Note:** These are rough allocations. As each tree is designed, the exact costs will be calibrated. The constraint is that the total AI budget across all units should remain ~30% of the full-game research budget (~13,000-15,000 SCIENCE).

## Implementation Notes

### UnlockRegistry Integration

AI unlocks are a different category from module tier techs. Current options:

1. **Same registry, namespaced strings** — e.g., `"AI_PROSPECTING_AUTO_COLLECTION"` alongside `"Spectroscopy"`. Simple, but the registry grows large.
2. **Separate AIUnlockRegistry** — dedicated system for AI capabilities with cost/prerequisite tracking built in. Cleaner separation.
3. **Unified ResearchManager** — both tier techs and AI unlocks become ResearchProject entries in a single queue. Most architecturally clean.

**Recommendation:** Option 3, implemented in Phase 6. The ResearchManager would own both tier unlocks and AI unlocks, with the UnlockRegistry becoming a read-only query interface ("is X unlocked?").

### UI Pattern

Each unit's view should have an "AI / Automation" panel (or tab) where the player can:
- See the AI tree for that unit type
- See which upgrades are purchased vs. available vs. locked
- Purchase the next upgrade (if prerequisites met and SCIENCE available)
- Toggle AI on/off per-unit

This is a shared UI component instantiated per unit type — not a separate screen.

## Cross-References

### Related Design Docs
| Document | Location | Relationship |
|----------|----------|-------------|
| Prospecting Master Design | `docs/design/prospecting/prospecting-master-design.md` | Section 11b: first concrete AI tree (source of this pattern) |
| Prospecting AI Default Mode | `docs/design/prospecting/ai-default-mode.md` | Detailed AI heuristics and efficiency penalties for prospecting |
| Research Module | `docs/design/research/README.md` | Research system that funds AI unlocks |
| Overall Roadmap | `ROADMAP_OVERALL.md` | Phase 6: Research & Technology |

### Source Code
| File | Relevant Code |
|------|--------------|
| `src/UnlockRegistry/unlock_registry.h` | Current stub tech system (will evolve to support AI unlocks) |
| `src/Unit/unit.cpp` | AI auto-management toggles in prospecting (~line 2162+) |
| `src/Colony/colony.h` | `strategicReserves[SCIENCE]` — the pool AI upgrades draw from |
