# Prospecting Module — Master Design Document

> Status: DRAFT
> Last Updated: 2026-04-01
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
| **Sweep** | Choose sweep depth/frequency | Confidence heat map over 5x5 grid | Where to invest sampling effort |
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

### Default Mode Behavior

| Stage | Default Behavior | Efficiency Penalty | Details |
|-------|-----------------|-------------------|---------|
| Sweep | Auto-depth: surface only | [?] Lower resolution | See [ai-default-mode.md](ai-default-mode.md) |
| Sampling | Predefined depth priority | [?] May miss optimal layers | See [depth-sampling-design.md](depth-sampling-design.md) |
| Tray | Auto-assign cheapest tools | [?] Fewer elements revealed | See [ai-default-mode.md](ai-default-mode.md) |
| Testing | Default tool chain (XRF only?) | [?] Misses light elements | See [ai-default-mode.md](ai-default-mode.md) |

### Open Questions — Multi-Scale Control

| Question | Status |
|----------|--------|
| Exact efficiency penalty numbers for default mode | [?] Needs playtesting |
| UI for switching between default and fine-control | [?] Toggle? Per-stage dropdown? See [ui-layout.md](ui-layout.md) |
| When does AI delegation unlock? (always? tier-gated?) | [?] Design decision needed |
| Can player switch modes mid-pipeline? | [?] Probably yes, but edge cases need thought |

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
- Player selects sweep depth via frequency control
- High frequency = detailed surface map (1 layer, high resolution)
- Low frequency = deep but blurry map (all layers, low resolution)
- Output: color-coded grid overlay (blue=uniform, yellow=moderate, red=anomaly)
- Each sweep costs energy
- Results have a **confidence level** — see [confidence-system.md](confidence-system.md)
- **[?]** Exact frequency bands and energy costs
- **[?]** How many sweeps before diminishing returns?

#### Phase 1: Sampling (Core Drilling)
- Player selects cell on grid + depth layer
- Produces a physical sample object placed in tray
- Each cell needs multiple sample types to complete survey profile
- Deeper layers require higher tiers
- See [depth-sampling-design.md](depth-sampling-design.md) for full depth system
- **[?]** Time cost per sample extraction
- **[?]** Maximum samples per cycle

#### Phase 2: Sample Tray (Inventory + Pipeline Design)
- Fixed-size tray (capacity scales with tier)
- Samples display initial data (visual characteristics from collection)
- Player designs processing pipeline per sample:
  - Which separation method? (Magnetic / Heavy mineral / Volatile extraction)
  - Which analysis tool? (XRF / LIBS / Fire Assay)
- In default mode: auto-assigned cheapest pipeline
- **[?]** Tray capacity per tier: T0=?, T1=?, T2=?, T3=?
- **[?]** Can samples be discarded to free slots?

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

- **Fire assay decision:** Use when high-value element suspected (He-3, rare minerals). Gives perfect data but loses the sample — can't apply other tools after.
- **Cross-referencing bonus:** Same tool on adjacent cells → +survey bonus for both (T1+)
- **[?]** Processing time per tool
- **[?]** Can multiple tools be applied sequentially to same sample?

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
- **[?]** When/how this view is displayed (separate tab? overlay?)

---

## 5. Confidence Level System

> Full design in [confidence-system.md](confidence-system.md)

### Framework (to be filled)

| Question | Decision |
|----------|----------|
| What does "confidence" measure? | [?] Accuracy of element readings? Certainty of deposit existence? Both? |
| Scale | [?] 0-100%? Discrete levels? |
| What affects confidence? | [?] Number of scans, tool quality, sample depth, sweep resolution |
| How does confidence affect gameplay? | [?] Gates extraction? Affects efficiency? Visual only? |
| Granularity | [?] Per-cell? Per-element? Per-sample? |
| How do multiple methods compose? | [?] Additive? Bayesian? Max? |
| Relationship to survey progress | [?] Is confidence a component of surveyProgress, or separate? |

---

## 6. Depth System

> Full design in [depth-sampling-design.md](depth-sampling-design.md)

### Layer Structure

| Layer | Index | Tier Required | Default Auto Priority | Contents |
|-------|-------|--------------|----------------------|----------|
| Surface | 0 | T0 | 1st | [?] Which resources concentrate here? |
| Shallow | 1 | T1 | 2nd | [?] |
| Mid | 2 | T2 | [?] | [?] |
| Deep | 3 | T3 | [?] | [?] |

### Open Questions — Depth

| Question | Status |
|----------|--------|
| Resource distribution model across depths | [?] — primary design task for [depth-sampling-design.md](depth-sampling-design.md) |
| Why would default mode skip a layer? (strategic incentive) | [?] — e.g., mid-layer has rare elements that auto-mode misses |
| Can player re-sample same cell/depth? | [?] |
| Does deeper drilling cost more energy/time? | [?] — probably yes, linearly or exponentially |
| Tray capacity per tier | [?] T0=4?, T1=8?, T2=10?, T3=12? (from Design 7 base) |

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
| **Sweep** | No sweep | Surface-only GPR | Frequency slider (surface→mid) | Full-depth GPR |
| **Sampling depths** | Surface only | +Shallow | +Mid | +Deep (all 4) |
| **Tray capacity** | [?] 4 | [?] 8 | [?] 10 | [?] 12 |
| **Analysis tools** | Visual only | +XRF | +LIBS | +Fire Assay |
| **Lab bench slots** | 1 concurrent | 2 concurrent | 3 concurrent | 4 concurrent |
| **Separation methods** | None | Magnetic only | +Heavy mineral | +Volatile extraction |
| **Pathfinder tips** | None | Basic ("nearby") | Directional arrows | Exact cell + abundance |
| **Stratigraphy** | None | 2-layer column | Cross-section correlation | Full 4-layer + auto-correlate |
| **AI/Default mode** | [?] | [?] | [?] | Full auto available |
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

- Survey progress accumulates from: sweep confidence + sample completeness + tool analysis results
- The exact formula mapping these inputs to `surveyProgress` is **[?]** — needs design
- Marked sites: player explicitly marks cells after reviewing results
- Objective bonus: retained from current system or redesigned — **[?]**

---

## 11. Gaps Inventory

### Must-Resolve Before Implementation

| # | Gap | Document | Priority |
|---|-----|----------|----------|
| 1 | Confidence system formal definition | [confidence-system.md](confidence-system.md) | HIGH |
| 2 | Resource distribution across depth layers | [depth-sampling-design.md](depth-sampling-design.md) | HIGH |
| 3 | Default mode efficiency penalties (numbers) | [ai-default-mode.md](ai-default-mode.md) | HIGH |
| 4 | Tray capacity per tier | This document, Section 9 | MEDIUM |
| 5 | surveyProgress accumulation formula (from new mechanics) | This document, Section 10 | HIGH |

### Must-Resolve Before Detailed Design

| # | Gap | Document | Priority |
|---|-----|----------|----------|
| 6 | AI delegation heuristics | [ai-default-mode.md](ai-default-mode.md) | MEDIUM |
| 7 | Sweep frequency bands and energy costs | This document, Section 4 | MEDIUM |
| 8 | Processing times per tool | This document, Section 4 | MEDIUM |
| 9 | Sample visual differentiation | [ui-layout.md](ui-layout.md) | LOW |

### Deferred to Future Phase

| # | Gap | Document | Dependency |
|---|-----|----------|-----------|
| 10 | Pathfinder element correlation rules | [resource-distribution-model.md](resource-distribution-model.md) | Needs geological coherence model |
| 11 | Clue chaining reward tuning | [resource-distribution-model.md](resource-distribution-model.md) | Needs pathfinder rules |
| 12 | Cross-referencing spatial patterns | This document, Section 4 | Needs geological coherence model |
| 13 | Stratigraphy visualization details | [ui-layout.md](ui-layout.md) | Needs depth model |
| 14 | Per-tool tier unlock costs | This document, Section 9 | Needs game balance pass |
