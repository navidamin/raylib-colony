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
- **[?]** When/how this view is displayed (separate tab? overlay?)

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
| **Sweep** | No sweep | Surface-only GPR | Frequency slider (surface→mid) | Full-depth GPR |
| **Sampling depths** | Surface only | +Shallow | +Mid | +Deep (all 4) |
| **Tray capacity** | 4 | 8 | 12 | 16 |
| **Analysis tools** | Visual only | +XRF | +LIBS | +Fire Assay |
| **Lab bench slots** | 1 concurrent | 2 concurrent | 3 concurrent | 4 concurrent |
| **Separation methods** | None | Magnetic only | +Heavy mineral | +Volatile extraction |
| **Pathfinder tips** | None | Basic ("nearby") | Directional arrows | Exact cell + abundance |
| **Stratigraphy** | None | 2-layer column | Cross-section correlation | Full 4-layer + auto-correlate |
| **AI/Default mode** | Basic auto (-20% conf) | Better targeting (-15%) | Uses sweep data (-10%) | Full auto (-5% conf) |
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

### Objective List (Resolved)

Each tier has 4-5 objectives. Completing all objectives in a tier grants a **tier completion bonus** on top of individual rewards.

**Reward timing principle:** Slot rewards come from "sustained usage" objectives (fill tray, achieve confidence, multi-depth sampling) — not "first use" objectives. The player should feel the tray constraint before earning relief. First-use objectives reward AI upgrades and preset unlocks instead.

#### T0 — Basics (Surface Sampling + Visual Inspection)

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 0-1 | "Collect your first core sample" | Drilling mechanic | — (gated tutorial) |
| 0-2 | "Visually inspect a sample" | Tool application | — (gated tutorial) |
| 0-3 | "Collect samples from 3 different cells" | Spatial coverage matters | AI auto-collect unlocked |
| 0-4 | "Fill your sample tray (4/4)" | Tray management, discard decisions | **+1 tray slot** |

*Player reaches 0-4 only after filling all 4 slots — they've hit the wall, made discard decisions or felt the squeeze. The +1 is relief.*

*T0 completion bonus: AI will auto-discard lowest-value samples when tray is full*

#### T1 — Sweep + XRF + Depth

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 1-1 | "Run your first GPR surface sweep" | Sweep mechanic, heat map reading | — (gated tutorial) |
| 1-2 | "Perform XRF analysis on a sample" | XRF tool (heavy element detection) | Early Structural preset unlock |
| 1-3 | "Achieve 'Moderate' confidence on any element" | Multi-measurement composition | **+1 tray slot** |
| 1-4 | "Sample from the Megaregolith layer" | Depth selection, Ti/ilmenite access | AI uses sweep data for targeting |
| 1-5 | "Cross-reference results between 2 adjacent cells" | Adjacency bonus mechanic | **+1 tray slot** |

*1-3 requires multiple measurements on the same element (probabilistic composition). By then, the player has been cycling through samples and feeling the 9-slot limit. 1-5 requires 2 adjacent cells fully analyzed — even more investment.*

*T1 completion bonus: AI prioritizes cells flagged by sweep instead of random targeting*

#### T2 — LIBS + Deep Geology + Frequency Diversity

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 2-1 | "Use LIBS to detect a light element (H, C, or O)" | LIBS advantage over XRF | Early Life Support preset unlock |
| 2-2 | "Sample from the Fractured Bedrock layer" | Deep sampling, water ice access | AI considers tool diversity |
| 2-3 | "Achieve 'High' confidence on any cell" | Tool diversity, probabilistic composition | **+1 tray slot** |
| 2-4 | "Run sweeps at 3 different frequency bands" | Frequency diversity, deep anomaly detection | **+1 tray slot** |
| 2-5 | "Apply 2 different tools to the same sample" | Sequential multi-tool pipeline | AI uses multi-tool pipelines |

*2-3 requires cell-level High confidence — multiple elements at Moderate+ from diverse tools. The player has been deep in tray management. 2-4 costs 190+ energy across 3 sweeps — real investment.*

*T2 completion bonus: AI auto-assigns Life Support preset for polar sites*

#### T3 — Fire Assay + Mastery

| # | Objective | Teaches | Reward |
|---|-----------|---------|--------|
| 3-1 | "Use Fire Assay on a He-3 candidate sample" | Destructive analysis risk/reward | Stratigraphy auto-correlate |
| 3-2 | "Sample all 4 depth layers in a single cell" | Complete depth coverage | **+2 tray slots** |
| 3-3 | "Achieve 'Certain' confidence on 3 different cells" | Mastery of full toolchain | AI confidence penalty reduced to 0% |
| 3-4 | "Complete a full stratigraphic column" | Stratigraphy correlation bonus | **+2 tray slots** |
| 3-5 | "Reach 90% survey progress on any cell" | Capstone — full prospecting mastery | Early access to Strategic preset |

*3-2 requires 4 samples in one cell = 25% of base tray. 3-4 requires all depths sampled AND analyzed. By these points the player is deep in tray logistics.*

*T3 completion bonus: Full AI autonomy mode — AI runs optimal pipelines with near-player efficiency*

#### Tray Slot Accounting

Base tray: 4/8/12/16 (T0/T1/T2/T3). Objective bonus slots stack:
- T0 rewards: +1 → effective 5 at T0, 9 at T1, etc.
- T1 rewards: +2 → effective 11 at T1 (if all complete)
- T2 rewards: +2 → effective 14 at T2
- T3 rewards: +4 → effective 20 at T3 (maximum)

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

### Also Resolved (Final Gaps, 2026-04-04)

| # | Gap | Resolution |
|---|-----|-----------|
| 13 | Exact objective list per tier | 4-5 objectives per tier (T0-T3), 18 total. Tutorial progression with mixed rewards (tray slots, AI upgrades, early unlocks). See Section 11 |
| 14 | 20 ore shape templates | 4 families × 5 templates (Angular Chunks, Crystalline Shards, Rounded Nodules, Layered Slabs) with depth-family affinity bias. See [ui-layout.md](ui-layout.md) |
| 15 | Cell aggregate confidence weighting | Abundance-weighted average, 5% threshold excludes trace elements. See [confidence-system.md](confidence-system.md) |

### Remaining — Must-Resolve Before Implementation

**None.** All must-resolve gaps have been addressed. The core pipeline design is implementation-ready.

### Remaining — Minor / Deferred

| # | Gap | Document | Status |
|---|-----|----------|--------|
| 6 | Manpower cost for drilling | [depth-sampling-design.md](depth-sampling-design.md) | Deferred |
| 7 | Sample archive vs active tray | [depth-sampling-design.md](depth-sampling-design.md) | Deferred |
| 8 | Stage tab visibility (all vs active only) | [ui-layout.md](ui-layout.md) | Deferred |

### Deferred to Future Phase

| # | Gap | Document | Dependency |
|---|-----|----------|-----------|
| 9 | Pathfinder element correlation rules | [resource-distribution-model.md](resource-distribution-model.md) | Needs geological coherence model |
| 10 | Clue chaining reward tuning | [resource-distribution-model.md](resource-distribution-model.md) | Needs pathfinder rules |
| 11 | Cross-referencing spatial patterns | This document, Section 4 | Needs geological coherence model |
| 12 | Stratigraphy visualization details | [ui-layout.md](ui-layout.md) | Needs depth model |
| 13 | Per-tool tier unlock costs | This document, Section 9 | Needs game balance pass |
