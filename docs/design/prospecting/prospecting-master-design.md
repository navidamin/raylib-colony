# Prospecting Module — Master Design Document

> Status: IMPLEMENTATION-READY
> Last Updated: 2026-04-04
> Depends on: [confidence-system.md](confidence-system.md), [depth-sampling-design.md](depth-sampling-design.md), [resource-distribution-model.md](resource-distribution-model.md), [ai-default-mode.md](ai-default-mode.md), [ui-layout.md](ui-layout.md)

---

## 1. Design Philosophy

The prospecting module redesign replaces the current system (composite noise formulas, calibration drift, campaign queues — too complex, not intuitive, not engaging) with a **Core Samples** approach grounded in real geological prospecting science.

**Three principles:**
1. **Scientifically coherent** — mechanics map to real sampling technologies (GPR, core drilling, XRF, LIBS, fire assay, stratigraphic logging)
2. **Multi-scale control** — every stage works automatically at reduced efficiency OR the player can fine-tune for better results
3. **Progressive disclosure** — complexity reveals itself as tiers unlock, not all at once

---

## 2. Prospecting Pipeline

Four sequential stages. Each stage has a default mode and a fine-control mode.

```
┌─────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  PHASE 0    │    │  PHASE 1     │    │  PHASE 2     │    │  PHASE 3     │
│  SWEEP      │───►│  SAMPLING    │───►│  SAMPLE TRAY │───►│  TESTING     │
│  (GPR Radar)│    │  (Core Drill)│    │  (Inventory)  │    │  (Analysis)  │
└─────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
       │                  │                   │                    │
   Confidence        Raw physical         Initial data       Element data
   heat map          samples              + pipeline plan    + survey progress
                                                             + pathfinder tips
```

### Stage Details

| Stage | Player Action | Output | Key Decision |
|-------|--------------|--------|--------------|
| **Sweep** | Choose sweep depth/frequency | Confidence heat map over prospecting grid | Where to invest sampling effort |
| **Sampling** | Choose cell + depth layer | Raw core sample added to tray | Which cells and depths to prioritize |
| **Sample Tray** | View initial data, design processing pipeline | Processing plan per sample | Which tools to use on which samples |
| **Testing** | Monitor (read-only during processing) | Element data, survey progress, pathfinder tips | Interpret results, plan next samples |

### Pipeline Iteration

The pipeline is **iterative**, not one-shot:
1. Sweep → identify promising zones
2. Sample a few cells → get initial results
3. Results + pathfinder tips → refine where to sample next
4. Repeat until survey progress is satisfactory
5. Begin extraction

### Spatial Model

#### Grid = Sub-Cell Resolution of One Planet Cell

The prospecting grid **subdivides the planet grid cell** where the sect is located. The planet's 20×20 grid stores coarse resource abundances per cell (e.g., "Fe = 0.7"). Prospecting reveals how those resources are distributed within the cell at higher resolution.

```
Planet View (20×20)              Prospecting Grid (scales with tier)
┌──┬──┬──┬──┐                    ┌──┬──┬──┬──┬──┐
│  │  │  │  │                    │.2│.3│.1│.4│.2│
├──┼──┼──┼──┤     ──zoom──►     ├──┼──┼──┼──┼──┤
│  │▓▓│  │  │                    │.3│.8│.9│.5│.3│  Sub-cell Fe values
├──┼──┼──┼──┤   "Fe = 0.7"      ├──┼──┼──┼──┼──┤  (average ≈ 0.7)
│  │  │  │  │                    │.1│.7│.5│.4│.2│
└──┴──┴──┴──┘                    ├──┼──┼──┼──┼──┤
                                 │.2│.3│.6│.8│.4│
                                 ├──┼──┼──┼──┼──┤
                                 │.1│.2│.3│.2│.1│
                                 └──┴──┴──┴──┴──┘
```

Prospecting = increasing resolution of existing planet data. The player already knows the planet cell is Fe-rich from the orbital survey. Prospecting reveals *where within that cell* the Fe concentrates (sub-cell clusters, gradients, deposits).

**Requires:** Sub-cell resource distribution generation in `ResourceManager`. Given a planet cell's resource values, procedurally generate NxN sub-cell distribution with spatial coherence (clusters, not random noise). See [resource-distribution-model.md](resource-distribution-model.md).

#### Grid Size Scales With Tier

Higher-tier equipment scans at finer resolution:

| Tier | Grid Size | Sub-Cells | Resolution |
|------|-----------|-----------|------------|
| T0 | 3×3 | 9 | Coarse — broad zones |
| T1 | 4×4 | 16 | Moderate — individual deposits visible |
| T2 | 5×5 | 25 | Good — deposit boundaries clear |
| T3 | 6×6 | 36 | Fine — precise deposit mapping |

When the grid expands at tier-up, previously surveyed sub-cells are re-divided. Existing survey data is preserved but the finer grid reveals detail that was invisible at coarser resolution.

#### Tray Capacity vs Sample Space

Grid growth and depth growth keep pace with tray capacity, maintaining consistent pressure:

| Tier | Grid | Depths | Total Possible | Tray | Batches for Full Coverage |
|------|------|--------|----------------|------|--------------------------|
| T0 | 3×3 | 1 | 9 | 4 | ~2 |
| T1 | 4×4 | 2 | 32 | 8 | ~4 |
| T2 | 5×5 | 3 | 75 | 12 | ~6 |
| T3 | 6×6 | 4 | 144 | 16 | ~9 |

### New Sect / New Colony Behavior

#### Tiers Are Per-Unit, Research Is Global

- **Module tier:** Per-unit. A new sect's prospecting module starts at T0.
- **Unlock techs:** Global (`UnlockRegistry`). A new unit can be upgraded immediately to whatever tier the player has researched, if they pay the resource cost.
- **AI research:** Global. If the player invested research tokens in "Sweep-Guided Targeting," every sect benefits from T0 — including new ones. This is the key payoff for AI investment: it scales across the empire.
- **Tutorial hints:** Shown once globally. New sects skip tutorial text — the player already knows the buttons.
- **Survey data:** Per-sect. New ground must be prospected from scratch. No shortcuts — the process must be done, but the tools and AI are available from the start if researched.

**Implication for AI investment:** A player who ignores AI research must manually prospect every new sect. A player who invested in AI research can set new sects to Default mode and let the AI handle prospecting while they focus elsewhere. The AI investment is an empire-scaling decision.

---

## 3. Multi-Scale Control System

```
   ENGAGEMENT SPECTRUM (per stage)

   [DEFAULT / AUTO]                              [FINE CONTROL]
        │                                              │
   AI handles decisions                     Player makes each choice
   Lower efficiency multiplier              Full efficiency potential
   Lower confidence in results              Higher confidence
   No per-sample decisions                  Per-sample tool selection
        │                                              │
        └──────────── Player toggles ──────────────────┘
```

### Control Granularity

| Level | Description | Example |
|-------|-------------|---------|
| **Global default** | All stages run automatically | "Prospect this area" — AI handles everything |
| **Per-stage override** | Player controls one stage, AI handles rest | Player does manual sampling, AI handles sweep and testing |
| **Per-sample fine-tune** | Player controls individual sample processing | Player selects LIBS for H2O-promising sample, fire assay for He-3 candidate |

### Default Mode Behavior (Resolved)

| Stage | Default Behavior | What It Misses | Penalty Type |
|-------|-----------------|----------------|-------------|
| Sweep | Surface-only, single frequency | Deep anomalies, frequency optimization | Missed opportunities |
| Sampling | Random cells, fixed depth priority (L0→L1→L3→L2) | Targeted sampling based on sweep data | Missed opportunities |
| Tray | Cheapest separation + cheapest tool | Element-specific tool matching | Missed opportunities |
| Testing | Cheapest available tool, no cross-referencing | Light elements, precise values, adjacency bonuses | Missed opportunities |

**AI is always available from T0**, improving with tier. Primary penalty is lack of strategic agency (random targeting, cheapest tools), plus a small confidence penalty (20% at T0, decreasing to 5% at T3). See [ai-default-mode.md](ai-default-mode.md) for full design.

### Mode Switching

Per-stage implicit override: interacting with a stage overrides auto for that stage. Global toggle in header sets baseline.

---

## 4. Science-Based Mechanics Integration

All mechanics from variants 7A-7E are incorporated. See [sampling-mechanics.md](sampling-mechanics.md) for full variant descriptions.

### Mechanics Matrix

| Mechanic | Source | Pipeline Stage | Role | Priority |
|----------|--------|---------------|------|----------|
| **GPR Radar Sweep** | 7C | Phase 0 (Sweep) | Pre-scan heat map, guides sampling | Core — implement first |
| **Core Drilling** | Base 7 | Phase 1 (Sampling) | Physical sample extraction at depth | Core — implement first |
| **Sample Tray** | Base 7 | Phase 2 (Tray) | Inventory with initial data display | Core — implement first |
| **Multi-Tool Analysis** | 7D | Phase 3 (Testing) | XRF / LIBS / Fire Assay per sample | Core — implement first |
| **Lab Bench Processing** | 7B | Phase 2→3 transition | Crush → Separate → Assay pipeline | Core — implement first |
| **Stratigraphy View** | 7E | Phase 3 visualization | Depth column + cross-section | Secondary — implement after core |
| **Pathfinder Clues** | 7A | Phase 3 output | Analysis tips after results | Future phase — needs resource model |
| **Cross-referencing** | 7D | Phase 3 bonus | Adjacent cell data comparison | Future phase — needs resource model |
| **Clue Chaining** | 7A | Phase 3 bonus | Trail bonus from following pathfinders | Future phase — needs resource model |

### Mechanic Details

#### Phase 0: Sweep (GPR Radar)
- **Continuous frequency slider** across 4 bands (shallow-detailed ↔ deep-blurry)
- High frequency = detailed surface map (1 layer, high resolution)
- Low frequency = deep but blurry map (all layers, low resolution)
- Output: color-coded grid overlay (blue=uniform, yellow=moderate, red=anomaly)
- Results have a **confidence level** — see [confidence-system.md](confidence-system.md)
- **One sweep per frequency band** — no same-frequency repeats, but multiple different frequencies allowed

**Sweep Frequency Bands & Energy Costs:**

| Frequency Band | Depth Penetration | Resolution | Energy Cost | Notes |
|---------------|-------------------|-----------|-------------|-------|
| High (Surface) | Regolith only | High detail | 30 energy | ~2 energy unit cycles. Reveals surface anomalies clearly |
| Medium-High | Regolith + Megaregolith | Good detail | 60 energy | Reveals Ti-bearing ilmenite zones |
| Medium-Low | Through Fractured Bedrock | Moderate detail | 100 energy | Key for finding water ice deposits |
| Low (Deep) | All 4 layers | Blurry | 150 energy | ~10 energy cycles. Premium cost for full-depth picture |

*Calibrated against energy unit production rate of 15.0f/cycle. Surface sweep is trivially cheap; deep sweep is a meaningful investment.*

#### Phase 1: Sampling (Core Drilling)
- Player selects cell on grid + depth layer
- Produces a physical sample object placed in tray
- Each cell needs multiple sample types to complete survey profile
- Deeper layers require higher tiers
- See [depth-sampling-design.md](depth-sampling-design.md) for full depth system
- **[?]** Time cost per sample extraction
- **[?]** Maximum samples per cycle

#### Phase 2: Sample Tray (Inventory + Pipeline Design)
- Fixed-size tray: **4 / 8 / 12 / 16** samples (T0/T1/T2/T3)
- Samples display initial data (visual characteristics from collection)
- **Samples can be discarded at any time for free** to reclaim tray slots
- Player designs processing pipeline per sample:
  - Which separation method? (Magnetic / Heavy mineral / Volatile extraction)
  - Which analysis tool? (XRF / LIBS / Fire Assay)
- **Pipeline presets** available: Structural, Life Support, Strategic, Full Survey (see [ui-layout.md](ui-layout.md))
- In default mode: auto-assigned cheapest pipeline

#### Phase 3: Testing (Multi-Tool Analysis)
- Samples enter processing (view-only while in progress)
- UI shows rotating sample icons with incoming result data stream
- Results reveal element data + contribute to survey progress
- **Pathfinder tips** appear in message bar after each analysis
- Tools available per tier:

| Tool | Reveals | Cost | Destructive? | Tier |
|------|---------|------|-------------|------|
| Visual inspection | Rock type, texture category | Free | No | T0 |
| XRF scan | Major elements (Fe, Si, Al, Ca, Ti) | Low energy | No | T1 |
| LIBS pulse | All elements incl. H, C, O | Moderate energy | No | T2 |
| Fire assay | Perfect concentration of ONE element | High energy | **Yes — consumes sample** | T3 |

- **Sequential multi-tool:** Yes, multiple tools can be applied to the same sample in sequence. **Fire assay ends the chain** (destructive — consumes sample, no further tools possible).
- **Processing time:** Near-instant for all tools except fire assay (which has a meaningful wait period).
- **Fire assay decision:** Use when high-value element suspected (He-3, rare minerals). Gives perfect data but loses the sample.
- **Cross-referencing bonus:** Same tool on adjacent cells → +survey bonus for both (T1+)

#### Pathfinder System (Future Phase)
- After analysis, tips appear: "Elevated arsenic detected — suggests iron deposit in adjacent cell"
- Displayed in side panel or message bar below the prospecting window
- **[?]** Exact pathfinder element correlations — see [resource-distribution-model.md](resource-distribution-model.md)
- **[?]** Clue chaining trail bonus values
- **Requires:** Geological coherence in resource map (adjacent cells must be related, not random)

#### Stratigraphy View (Secondary)
- Core samples from different depths build a vertical column per cell
- Completed columns (all depth layers sampled) reveal full geological model
- Adjacent completed columns show cross-section correlation lines
- Correlated layers → bonus survey progress
- **Display:** Side panel that appears next to the grid when hovering/selecting a cell with ≥2 depth samples. Panel slides in from the right, compressing the grid. See [ui-layout.md](ui-layout.md) for layout.

---

## 5. Confidence Level System (Resolved)

> Full design in [confidence-system.md](confidence-system.md)

### Framework

| Question | Decision |
|----------|----------|
| What does "confidence" measure? | Accuracy/certainty of element readings and deposit knowledge |
| Scale | **Hybrid:** 0-100% internal, 5 discrete display levels (Very Low / Low / Moderate / High / Certain) |
| What affects confidence? | Number of scans, tool quality, tool diversity, re-sampling |
| How does confidence affect gameplay? | **Modifies survey progress contribution** — higher confidence = more progress per result |
| Granularity | **Hierarchical:** cell heat map → per-element breakdown → per-depth breakdown |
| How do multiple methods compose? | **Probabilistic:** `conf = 1 - (1-c1)(1-c2)...` — rewards tool diversity |
| Decay? | **No decay** — confidence is permanent |
| Relationship to survey progress | Confidence is a **multiplier** on survey progress gain (Very Low=0.2x, Certain=1.0x) |

---

## 6. Depth System (Resolved)

> Full design in [depth-sampling-design.md](depth-sampling-design.md)

### Layer Structure (Lunar Geology Model)

| Layer | Index | Tier | Default Auto Priority | Characteristic Resources |
|-------|-------|------|----------------------|------------------------|
| Regolith | 0 | T0 | 1st | Fe, Si, Al, Ca (bulk), trace solar-wind H, He |
| Megaregolith | 1 | T1 | 2nd | Ti (ilmenite), higher metal grades, trapped volatiles |
| Fractured Bedrock | 2 | T2 | **4th (skipped by AI)** | H₂O (ice in fractures), mineral veins, concentrated ores |
| Intact Bedrock | 3 | T3 | 3rd | He-3, rare minerals, pristine composition data |

**AI skips fractured bedrock** — it follows the "easy access" heuristic and doesn't know to look for water in fractures. A player who understands geology will prioritize it when water is needed.

### Key Decisions

| Question | Resolution |
|----------|-----------|
| Distribution model | Geologically realistic (lunar geology profiles) |
| Drilling cost | Stepped per layer with tier discounts (energy only) |
| Drill time | **Flat short time** for all depths (cost difference is energy only) |
| Tray capacity | **4 / 8 / 12 / 16** (T0/T1/T2/T3) |
| Re-sampling | Yes, with diminishing returns: `baseGain / sqrt(n)` |
| Sample discard | Free discard anytime |

---

## 7. Resource Distribution Model

> Full design in [resource-distribution-model.md](resource-distribution-model.md)

### Requirements

| Requirement | Purpose | Current Status |
|-------------|---------|---------------|
| Element distribution across planet grid | Base resource map | **Exists** in `ResourceManager` (cluster-based generation) |
| Element distribution across depth layers | Depth sampling incentives | **[?]** Not designed |
| Pathfinder correlations (element A → element B nearby) | 7A mechanic | **[?]** Correlation rules needed |
| Geological coherence (adjacent cells related) | Cross-referencing, stratigraphy | **[?]** Current grid may be per-cell random |
| Clue chaining spatial rules | 7A trail bonus | **[?]** How do deposits cluster? |

### Dependency Note
Pathfinder and clue chaining mechanics are **deferred to a future development phase** because they require a coherent resource distribution model that supports spatial correlations. The core pipeline (sweep → sample → tray → test) can be implemented independently.

---

## 8. Prospecting UI Concept

> Full design in [ui-layout.md](ui-layout.md)

### Initial View Structure

```
┌─────────────────────────────────────────────────────────────┐
│  PROSPECTING MODULE                    [Default ▼] [AI ◉]  │
├────────────┬────────────┬────────────┬──────────────────────┤
│  ◉ Sweep   │  ○ Tray    │  ○ Lab     │   Stage selector     │
│  (Phase 0) │  (Phase 2) │  (Phase 3) │                      │
├────────────┴────────────┴────────────┴──────────────────────┤
│                                                             │
│  ┌─ Active Stage View ─────────────────────────────────┐   │
│  │                                                      │   │
│  │  [Stage-specific content here]                       │   │
│  │                                                      │   │
│  │  Sweep: 5x5 grid with confidence heat map            │   │
│  │  Tray:  Sample icons with initial data               │   │
│  │  Lab:   Processing pipeline + incoming results       │   │
│  │                                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ Sample Icons (all stages) ──────────────────────────┐  │
│  │  [?] Rotating icons per stage                         │  │
│  │  [?] Visual differentiation: color=element? shape=    │  │
│  │      depth? glow=confidence?                          │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  Pathfinder Tips / Message Bar                              │
│  "Elevated Ti detected — industrial-grade ilmenite likely   │
│   in adjacent cells to the south"                           │
└─────────────────────────────────────────────────────────────┘
```

### Sample Visual Concepts
- Samples in **Tray** stage: show initial data (from sweep/visual), can be interacted with
- Samples in **Lab** stage: show processing animation + incoming data stream (view-only)
- **[?]** Varied shapes representing different depths? Different rock types?
- **[?]** Real-time data stream visualization or batch result delivery?

---

## 9. Tier Progression

| Feature | T0 | T1 | T2 | T3 |
|---------|----|----|----|----|
| **Grid resolution** | 3×3 (9 cells) | 4×4 (16 cells) | 5×5 (25 cells) | 6×6 (36 cells) |
| **Sweep** | No sweep | Surface-only GPR | Frequency slider (surface→mid) | Full-depth GPR |
| **Sampling depths** | Surface only | +Megaregolith | +Fractured Bedrock | +Intact Bedrock (all 4) |
| **Tray capacity** | 4 | 8 | 12 | 16 |
| **Analysis tools** | Visual only | +XRF | +LIBS | +Fire Assay |
| **Lab bench slots** | 1 concurrent | 2 concurrent | 3 concurrent | 4 concurrent |
| **Separation methods** | None | Magnetic only | +Heavy mineral | +Volatile extraction |
| **Pathfinder tips** | None | Basic ("nearby") | Directional arrows | Exact cell + abundance |
| **Stratigraphy** | None | 2-layer column | Cross-section correlation | Full 4-layer + auto-correlate |
| **AI/Default mode** | Base auto (research-gated) | Research-gated | Research-gated | Research-gated |
| **Cross-referencing** | None | Unlocked (adjacent bonus) | Multi-cell patterns | Auto-cross-reference |

---

## 10. Connection to Extraction Pipeline

**Prospecting output → Extraction input (unchanged)**

```
                    PROSPECTING                          EXTRACTION
              ┌─────────────────────┐            ┌──────────────────┐
              │ surveyProgress      │───────────►│ scanMultiplier   │
              │ (0.0 - 1.0)        │            │ = 0.35 + 0.65*sp │
              │                     │            │ + 0.15 if marked  │
              │ markedSites[]       │───────────►│ × objectiveBonus │
              └─────────────────────┘            └──────────────────┘
```

### Survey Progress Formula (Resolved)

Survey progress is **component-based**, with each component contributing to a portion of the total:

```
surveyProgress = sweepComponent + sampleComponent + testingComponent

Where:
  sweepComponent  = 0-20% (from GPR sweep coverage and frequency diversity)
  sampleComponent = 0-50% (from core sample collection across cells and depths)
  testingComponent = 0-30% (from analysis tool results)

Each component's contribution is scaled by confidence:
  actualGain = baseGain × confidenceMultiplier
  
  Confidence multipliers:
    Very Low (0-20%):   0.2x
    Low (21-40%):       0.4x
    Moderate (41-60%):  0.7x
    High (61-80%):      0.9x
    Certain (81-100%):  1.0x
```

- Marked sites: player explicitly marks cells after reviewing results
- Objective bonus: retained from current system

---

## 11. Objectives System

Prospecting objectives guide the player through mechanics progressively.

### Design Decisions (Resolved)

| Question | Decision |
|----------|----------|
| Generation | **Tier-locked tutorial objectives** — fixed per tier, teach new tools/mechanics |
| Deadlines | **None** — objectives persist indefinitely until completed |
| Rewards | **Mixed capability unlocks** per objective type |

### Reward Types

| Reward Category | Examples |
|----------------|---------|
| Tray bonus slots | +1 or +2 tray capacity |
| AI behavior upgrades | AI uses better targeting, considers sweep data |
| Early tool/preset unlock | Access to a tool or preset one tier early |

### Reward Model

Three reward sources, cleanly separated:

| Source | What It Gives | Scope |
|--------|-------------|-------|
| **Tier upgrades** | Base tray capacity, tools, depths, grid resolution | Per-unit |
| **Objectives** | Tray bonus slots, pipeline presets | Per-unit |
| **Research tokens** | AI capabilities | Global (all sects) |

- **Tutorial objectives** (first use of each tool) teach mechanics. **No payoff** — the player already received the tool from the tier upgrade.
- **Achievement objectives** (sustained usage milestones) give tray slots or presets. These require the player to have worked within the current constraints.
- **AI capabilities** are purchased from the colony research token pool. They apply globally to all sects. See Section 11b.

### Objective List (Resolved)

**Reward timing principle:** Slot rewards come from "sustained usage" objectives — not "first use" objectives. The player should feel the tray constraint before earning relief.

#### T0 — Basics (Surface Sampling + Visual Inspection)

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 0-1 | "Collect your first core sample" | Drilling mechanic | — (tutorial) |
| 0-2 | "Visually inspect a sample" | Tool application | — (tutorial) |
| 0-3 | "Collect samples from 3 different cells" | Spatial coverage | — (tutorial) |
| 0-4 | "Fill your sample tray (4/4)" | Tray management | **+1 tray slot** |

*Tutorial hints for 0-1 through 0-3 are shown once globally. New sects skip them but still must complete the process.*

#### T1 — Sweep + XRF + Depth

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 1-1 | "Run your first GPR surface sweep" | Sweep + heat map | — (tutorial) |
| 1-2 | "Perform XRF analysis on a sample" | XRF tool | — (tutorial) |
| 1-3 | "Analyze a sample with Magnetic separation + XRF" | Structural workflow | **Structural preset** |
| 1-4 | "Achieve 'Moderate' confidence on any element" | Multi-measurement composition | **+1 tray slot** |
| 1-5 | "Cross-reference results between 2 adjacent cells" | Adjacency bonus | **+1 tray slot** |

*Structural preset earned by manually performing the workflow once — now it's a one-click button.*

#### T2 — LIBS + Deep Geology + Frequency Diversity

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 2-1 | "Use LIBS to detect a light element (H, C, or O)" | LIBS advantage over XRF | — (tutorial) |
| 2-2 | "Detect H₂O using LIBS on a Fractured Bedrock sample" | Volatile workflow | **Life Support preset** |
| 2-3 | "Achieve 'High' confidence on any cell" | Tool diversity | **+1 tray slot** |
| 2-4 | "Run sweeps at 3 different frequency bands" | Frequency diversity | **+1 tray slot** |
| 2-5 | "Apply 2 different tools to the same sample" | Multi-tool pipeline | — (tutorial) |

*Life Support preset earned by discovering the volatile detection workflow firsthand.*

#### T3 — Fire Assay + Mastery

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 3-1 | "Use Fire Assay on a sample" | Destructive analysis risk/reward | — (tutorial) |
| 3-2 | "Confirm He-3 via Fire Assay in Intact Bedrock" | Strategic resource workflow | **Strategic preset** |
| 3-3 | "Sample all 4 depth layers in a single cell" | Complete depth coverage | **+2 tray slots** |
| 3-4 | "Complete a full stratigraphic column" | Stratigraphy correlation | **+2 tray slots** |
| 3-5 | "Reach 90% survey progress on any cell" | Capstone mastery | **Stratigraphy auto-correlate** |

*Strategic preset earned by completing the He-3 discovery workflow. Auto-correlate earned at mastery — highlights cross-section patterns the player previously had to spot manually.*

#### Tray Slot Accounting

Base tray: 4/8/12/16 (T0/T1/T2/T3). Objective bonus slots per tier:
- T0: +1 → effective 5
- T1: +2 → effective 10
- T2: +2 → effective 14
- T3: +4 → effective 20 (maximum)

---

## 11b. AI Capabilities (Research-Gated)

AI capabilities are **purchased from the colony research token pool**, not earned from objectives. They apply **globally to all sects** — a key payoff for research investment.

### Research Economy Context

AI prospecting costs are calibrated against the full research budget:

| Parameter | Value | Source |
|-----------|-------|--------|
| Research unit output | 5 SCIENCE/tick, 100 SCIENCE/day | `unit.cpp` ResearchPointsPerTick |
| Early game (1 unit) | ~100/day | First research unit |
| Mid game (2-3 units) | ~200-300/day | Colony expansion |
| Late game (3-5 units) | ~300-500/day | Mature research infrastructure |
| Estimated full game | ~200-300 game-days | — |
| Total research budget | ~30,000-60,000 SCIENCE | Over a full playthrough |

**Budget allocation across all research categories:**

| Category | Share | Tokens (~45K avg) | What It Covers |
|----------|-------|-------------------|----------------|
| Extraction tech tree | ~25% | ~11,000 | 14 tier unlock techs (Spectroscopy, MechanizedDrilling, etc.) |
| Other unit tech trees | ~25% | ~11,000 | Farming, Energy, Manufacturing, Research tier unlocks |
| **Prospecting AI** | **~15%** | **~6,400** | **8 AI automation upgrades (this section)** |
| Future AI systems | ~15% | ~6,500 | Other unit automation (deferred) |
| Colony-wide research | ~20% | ~9,000 | Colony techs (deferred) |

Prospecting AI is a significant but not dominant research investment — about 15% of total budget.

### AI Research Projects

| Research Project | Cost (SCIENCE) | Research-Days (1 unit) | Effect | Prerequisite |
|-----------------|---------------|----------------------|--------|-------------|
| **Basic Automation** | 0 (Free) | 0 | AI drills random cells, applies visual inspection, stops when tray full | None (available T0) |
| **Auto-Collection** | 200 | 2 days | AI collects samples without player input in Default mode | Basic Automation |
| **Auto-Discard** | 200 | 2 days | AI replaces lowest-value sample when tray is full instead of stopping | Auto-Collection |
| **Sweep-Guided Targeting** | 500 | 5 days | AI targets cells flagged by sweep heat map instead of random selection | T1 tools unlocked |
| **Tool Matching** | 500 | 5 days | AI selects XRF for heavy elements, LIBS for light, instead of cheapest | T2 tools unlocked |
| **Multi-Tool Pipelines** | 500 | 5 days | AI applies sequential tools to samples (XRF then LIBS) instead of single tool | Tool Matching |
| **Context-Aware Presets** | 1000 | 10 days | AI selects appropriate preset based on site characteristics (polar → Life Support) | Multi-Tool Pipelines |
| **Precision Calibration** | 1000 | 10 days | AI confidence penalty reduced from -20% to 0% | Context-Aware Presets |
| **Full Autonomy** | 2500 | 25 days | AI runs near-player-quality decisions: optimal targeting, all tools, strategic depth | All above |
| **TOTAL** | **6,400** | **64 days (1 unit)** | | |

**Pacing milestones:**

| Milestone | Cumulative Cost | With 1 Unit | With 2 Units | What Changes |
|-----------|----------------|-------------|-------------|--------------|
| Basic convenience (Auto-Collection + Auto-Discard) | 400 | 4 days | 2 days | AI handles sample management |
| Smart targeting (+ Sweep-Guided) | 900 | 9 days | 4.5 days | AI stops being random, uses survey data |
| Tool intelligence (+ Tool Matching + Pipelines) | 1,900 | 19 days | 9.5 days | AI uses right tools, multi-step analysis |
| Near-autonomous (+ Context-Aware + Precision) | 3,900 | 39 days | 19.5 days | AI adapts to site, no confidence penalty |
| Full autonomy | 6,400 | 64 days | 32 days | Fire and forget across all sects |

**Empire scaling:** A player who invests in AI research can set new sects to Default mode and let the AI handle prospecting across the colony. A player who skips AI research must manually prospect every sect. This makes AI research an empire-management decision, not just a convenience toggle.

**Design rationale:** Early convenience upgrades (Auto-Collection, Auto-Discard) are cheap — a player gets quality-of-life improvements within 2-4 days. The mid-tier intelligence upgrades (targeting, tools) unlock over the first few game-weeks. Full Autonomy is a late-game capstone requiring serious research commitment. With 2 Research units, the full tree completes in ~32 days — roughly one game-month of dedicated research.

---

## 12. Gaps Inventory

### Resolved (Q&A Session 2026-04-02)

| # | Gap | Resolution |
|---|-----|-----------|
| 1 | Confidence system formal definition | Hybrid 0-100% / 5 levels, probabilistic composition, hierarchical granularity |
| 2 | Resource distribution across depth layers | Geologically realistic lunar model (4 layers) |
| 3 | Default mode efficiency penalties | Missed opportunities + small confidence penalty (20% max, decreasing with tier) |
| 4 | Tray capacity per tier | 4 / 8 / 12 / 16 |
| 5 | surveyProgress accumulation formula | Component-based: sweep (0-20%) + samples (0-50%) + testing (0-30%), scaled by confidence |
| 6 | AI delegation heuristics | Always available, random targeting + cheapest tools, improves with tier |
| 7 | Processing times per tool | Near-instant except fire assay |
| 8 | Sample visual differentiation | Procedural ore shapes, element-colored, glow=confidence, rings=depth |
| 9 | Pipeline mechanics | Continuous sweep slider, sequential multi-tool, fire assay ends chain, 4 presets |
| 10 | Objectives system | Tier-locked tutorial objectives, no deadlines, mixed capability rewards |

### Also Resolved (Energy Costs, 2026-04-02)

| # | Gap | Resolution |
|---|-----|-----------|
| 11 | Sweep frequency bands and energy costs | 4 bands: 30/60/100/150 energy (High→Low frequency) |
| 12 | Drilling energy costs | 15/30/50/75 base per layer, with tier discounts (calibrated to 15.0f/cycle production) |

### Also Resolved (Final Gaps + Structural Rework, 2026-04-04)

| # | Gap | Resolution |
|---|-----|-----------|
| 13 | Exact objective list per tier | Tutorials (no payoff) + achievement objectives (tray slots, presets). See Section 11 |
| 14 | 20 ore shape templates | 4 families × 5 templates with depth-family affinity bias. See [ui-layout.md](ui-layout.md) |
| 15 | Cell aggregate confidence weighting | Abundance-weighted average, 5% threshold. See [confidence-system.md](confidence-system.md) |
| 16 | AI reward source | AI capabilities research-gated (colony token pool), not objective-linked. See Section 11b |
| 17 | Prospecting grid spatial model | Grid subdivides one planet cell. Size scales with tier (3×3 → 6×6). See Section 2 |
| 18 | New sect/colony behavior | Process from scratch, tutorials shown once globally, AI research carries over. See Section 2 |

### Also Resolved (UI Layout Decisions, 2026-04-30)

| # | Gap | Resolution |
|---|-----|-----------|
| 19 | Stratigraphy view location | Side panel on hover/select (appears next to grid when cell has ≥2 depth samples). See [ui-layout.md](ui-layout.md) |
| 20 | Font scaling for prospecting UI | Same FS() as extraction view (1.30x at 48pt). See [ui-layout.md](ui-layout.md) |
| 21 | Objectives panel location | Bottom collapsible section below message bar. Collapsed by default, shows count. See [ui-layout.md](ui-layout.md) |

### Remaining — Must-Resolve Before Implementation

| # | Gap | Document | Priority |
|---|-----|----------|----------|
| 1 | ~~Sub-cell resource distribution generation~~ | [resource-distribution-model.md](resource-distribution-model.md) | **Resolved** — 3-layer model (provinces + deposits + Perlin sub-cells) |
| 2 | ~~AI research token costs~~ | This document, Section 11b | **Resolved** — 0/200/500/1000/2500 SCIENCE scale, 6,400 total tree cost, calibrated against full research economy (~15% of budget) |

### Remaining — Minor / Deferred

| # | Gap | Document | Status |
|---|-----|----------|--------|
| 3 | Manpower cost for drilling | [depth-sampling-design.md](depth-sampling-design.md) | Deferred |
| 4 | Sample archive vs active tray | [depth-sampling-design.md](depth-sampling-design.md) | Deferred |
| 5 | Stage tab visibility (all vs active only) | [ui-layout.md](ui-layout.md) | Deferred |
| — | ~~Font scaling~~ | [ui-layout.md](ui-layout.md) | **Resolved** (gap #20) |
| — | ~~Objectives panel location~~ | [ui-layout.md](ui-layout.md) | **Resolved** (gap #21) |

### Deferred to Future Phase

| # | Gap | Document | Dependency |
|---|-----|----------|-----------|
| 6 | Pathfinder element correlation rules | [resource-distribution-model.md](resource-distribution-model.md) | Needs geological coherence model |
| 7 | Clue chaining reward tuning | [resource-distribution-model.md](resource-distribution-model.md) | Needs pathfinder rules |
| 8 | Cross-referencing spatial patterns | This document, Section 4 | Needs geological coherence model |
| 9 | ~~Stratigraphy visualization details~~ | ~~[ui-layout.md](ui-layout.md)~~ | **Resolved** (gap #19) — side panel on hover/select |
| 10 | Per-tool tier unlock costs | This document, Section 9 | Needs game balance pass |
