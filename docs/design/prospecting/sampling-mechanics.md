# Sampling Mechanics — Science Review & Design Variants

> Status: DRAFT
> Last Updated: 2026-04-01
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Part 1: Real Sampling Technologies → Game Mechanics

Concise review of prospecting/sampling methods from planetary science and mining geology, with their game mechanic translations.

### 1. Core Drilling (Apollo drive tubes, rotary-percussion)

Cylindrical samples extracted at specific depths. Deeper = more expensive, slower, but reveals stratigraphy. Drill parameters (speed, force, bit type) affect sample quality. Too aggressive = disturbed sample, lost stratification.

**Game translation:** Depth selection with quality tradeoff. Player chooses which layer to sample; deeper costs more energy/time but reveals valuable subsurface resources.

*Sources: [Lunar Regolith Sampling Technologies Review](https://link.springer.com/article/10.1007/s11214-025-01239-6), [Apollo Sample Collection](https://apollo11space.com/how-lunar-regolith-samples-were-collected-during-the-apollo-missions/)*

### 2. LIBS (Laser-Induced Breakdown Spectroscopy — Curiosity/Perseverance)

Fires laser pulses at rock from distance. First pulses blast away dust (useless), subsequent pulses read clean surface. More pulses = better statistics, less noise. Each pulse reveals a spectrum matched against known element signatures.

**Game translation:** Multi-pulse refinement. Analysis tool that reveals all elements including light ones (H, C, O) that XRF misses. Moderate cost, high information yield.

*Sources: [LIBS on Mars — ChemCam, SuperCam, MarSCoDe](https://www.mdpi.com/2075-163X/15/8/882), [Perseverance LIBS Analysis](https://www.spectroscopyonline.com/view/new-study-reveals-how-nasa-s-perseverance-rover-uses-libs-for-precise-mars-rock-analysis)*

### 3. Portable XRF (X-Ray Fluorescence)

Point-and-shoot surface analysis. Instant results, no sample prep, but can't detect light elements (H, C, N, O) or low concentrations. Non-destructive.

**Game translation:** Quick free preview with blind spots. Sees most elements fast and cheap but misses critical light elements — player must decide if XRF data is sufficient or if LIBS is worth the extra cost.

*Sources: [Bruker Portable XRF for Mining](https://www.bruker.com/en/products-and-solutions/elemental-analyzers/handheld-xrf-spectrometers/handheld-xrf-applications/geoscience.html)*

### 4. GPR (Ground Penetrating Radar)

Sends radio pulses underground, reads echoes from layer boundaries. Key tradeoff: high frequency = high resolution but shallow; low frequency = deep but blurry. Non-invasive area scan.

**Game translation:** Frequency/depth slider for pre-scan phase. Area preview before committing to expensive drilling. Resolution-depth tradeoff is the player's decision.

*Sources: [GPR for Mineral Exploration](https://www.sciencedirect.com/science/article/abs/pii/S0926985111002126), [GPR Subsurface Mapping](https://geologyscience.com/geology-branches/geophysics/ground-penetrating-radar-gpr/)*

### 5. Pathfinder Elements

In real prospecting, you don't look for gold directly — you look for arsenic, antimony, bismuth which travel with gold. Finding indicator elements tells you the target deposit is nearby.

**Game translation:** Indirect clue system. Sample results include tips about what might be in adjacent cells. Creates a treasure-hunt layer on top of collection.

*Sources: [Pathfinder Elements in Geochemical Prospecting](https://www.intechopen.com/chapters/81376)*

### 6. Sample Preparation Chain (crush → separate → assay)

Raw field samples are rarely analyzed directly. Processing: crushing/grinding → heavy mineral separation → chemical assay. Each step costs time/money but increases data quality.

**Game translation:** Post-collection processing pipeline. Raw samples yield minimal data; processed samples yield detailed element breakdowns. Player manages processing queue.

*Sources: [Mining Sampling Methods](https://www.911metallurgist.com/blog/mining-geology-sampling-methods-channel-chip-core/), [Geological Sampling Methods](https://kocurekindustries.com/7-types-of-geological-sampling-methods-and-when-to-use-each)*

### 7. QA/QC Protocol (Standards, Blanks, Duplicates)

Real labs insert known-composition "standards" into sample batches to verify instrument accuracy. Blanks detect contamination. Duplicates check precision. If QA fails, the whole batch is suspect.

**Game translation:** Calibration/verification mechanic. Could tie into confidence system — uncalibrated instruments produce lower-confidence results.

*Sources: [Rock & Soil Sampling in Exploration](https://www.geologyforinvestors.com/rock-and-soil-sampling-the-key-to-most-exploration-projects/)*

### 8. Channel vs Grab vs Composite Sampling

Different collection strategies: grab (quick, single point, biased), channel (cut continuous strip, representative but slow), composite (many small chips averaged, good coverage).

**Game translation:** Sampling strategy choice. Quick samples give biased data; thorough samples take longer but are more representative.

*Sources: [Soil & Rock Chip Sampling in Mineral Exploration](https://rangefront.com/blog/what-is-soil-sampling/)*

---

## Part 2: Design Evolution

### Base Design: "Core Samples" (Design 7)

Each scan produces a physical "core sample" that the player collects in a sample tray. Samples are examined and categorized. Completing a sample set for a cell maximizes its survey progress.

**Core loop:** Collect samples → fill tray → examine → complete sets → survey progress increases

**Sample types needed per cell** (3-5 depending on tier):
- Surface regolith
- Subsurface core
- Mineral grain
- Volatile trace
- Depth profile

**Tray:** Limited inventory (scales with tier). Forces prioritization of which cells to focus on.

**Anomaly samples:** Rare bonus samples that provide extraction bonuses or unlock objectives.

**Tier progression:**
- T0: 1 sample type (surface). Small tray
- T1: 3 sample types. Medium tray. Basic examination
- T2: 4 sample types. Larger tray. Anomaly samples can appear
- T3: All 5 types. Full tray. Auto-collect mode

---

## Part 3: Design Variants (each adds 1-2 mechanics to Base Design 7)

### Variant 7A: "Pathfinder Clues" — Indirect Discovery

**Added mechanics:**
1. **Pathfinder element system** — analysis results include indicator elements that point to adjacent cells
2. **Clue chaining** — following pathfinder trails from cell to cell builds a stacking survey bonus

**How it works:**
- Sample analysis reveals pathfinder indicators (e.g., elevated arsenic → iron deposit nearby)
- Indicators appear as directional hints on the grid (arrows, warm/cold)
- Following the chain: each correct follow-up sample in the indicated direction gives +5% survey bonus (stacking)
- Breaking the chain (sampling unindicated cell) resets bonus

**Tier progression:**
- T0: No pathfinder clues
- T1: Clues appear but only show "nearby" (no direction)
- T2: Directional arrows. Trail bonus active
- T3: Exact cell + expected abundance range

**Integration point:** Phase 3 output (after analysis results)
**Dependency:** Requires geological coherence in resource distribution model
**Schedule:** Future development phase

---

### Variant 7B: "Lab Bench" — Sample Processing Pipeline

**Added mechanics:**
1. **Post-collection processing chain** — raw samples go through crush → separate → assay
2. **Batch processing** — lab processes multiple samples concurrently (slots scale with tier)

**Processing pipeline:**
```
[Raw Sample] → [Crush/Grind] → [Separation Method] → [Assay] → [Analyzed Sample]
                 (auto, uses       (player chooses)     (auto)
                  energy)
```

**Separation methods** (player chooses per sample):
| Method | Reveals | Speed | Cost |
|--------|---------|-------|------|
| Magnetic separation | Fe, Ti, Al | Fast | Low |
| Heavy mineral separation | Rare/dense elements | Slow | High |
| Volatile extraction | H2, water, gases | Moderate | Moderate |

**Batch mechanics:**
- Lab bench processes N samples simultaneously (T1=2, T2=3, T3=4)
- Same cell + different separation methods → combined data = full element picture
- While batch processes, player plans next collection targets

**Integration point:** Phase 2→3 transition
**Schedule:** Core — implement with base system

---

### Variant 7C: "Radar Scout" — Two-Phase Prospecting

**Added mechanics:**
1. **GPR pre-scan phase** — radar sweep reveals subsurface anomaly map before sampling
2. **Frequency/depth tradeoff** — player chooses radar frequency (resolution vs depth)

**Two phases:**
- **Phase 1 — Radar sweep:** Click "SCAN" to sweep 5x5 grid. Produces color overlay:
  - Blue = uniform (low interest)
  - Yellow = density variation (moderate)
  - Red = strong anomaly (high interest, worth sampling)
- **Phase 2 — Targeted sampling:** Guided by radar overlay. Red-anomaly cells yield higher-quality samples

**Frequency slider:**
| Setting | Resolution | Depth | Energy |
|---------|-----------|-------|--------|
| High frequency | Detailed | Surface only | Low |
| Medium frequency | Moderate | Surface + Shallow | Medium |
| Low frequency | Blurry | All layers | High |

**Integration point:** Phase 0 (before sampling begins)
**Schedule:** Core — implement with base system

---

### Variant 7D: "Multi-Tool Analysis" — Instrument Selection

**Added mechanics:**
1. **Multiple analysis instruments** per sample — each reveals different data at different costs
2. **Cross-referencing** — same tool on adjacent cells improves data quality for both

**Tool table:**

| Tool | Reveals | Cost | Time | Destructive? | Tier |
|------|---------|------|------|-------------|------|
| Visual inspection | Rock type, texture | Free | Instant | No | T0 |
| XRF scan | Major elements (Fe, Si, Al, Ca, Ti) | Low | Fast | No | T1 |
| LIBS pulse | All elements incl. H, C, O | Moderate | Moderate | No | T2 |
| Fire assay | Perfect concentration of ONE chosen element | High | Slow | **Yes** | T3 |

**Fire assay dilemma:** Consumes the sample. Use when:
- H2O-promising region → need definitive water confirmation
- He-3 possibility → high profit potential justifies destruction
- Hydrogen presence indicated → volatile extraction path depends on confirmation

**Cross-referencing:**
- XRF on cell (2,3) + XRF on cell (2,4) → if shared elements detected, both get +10% survey bonus
- Encourages systematic sampling patterns

**Integration point:** Phase 3 (testing)
**Schedule:** Core — implement with base system

---

### Variant 7E: "Stratigraphic Column" — Geological Modeling

**Added mechanics:**
1. **Layer-ordered sample arrangement** — samples snap into depth positions in a vertical column
2. **Cross-section correlation** — adjacent columns with matching layers show correlation lines + bonus

**Stratigraphic column (per cell):**
```
┌─────────────┐
│   Surface    │ ← sample slot 0
├─────────────┤
│   Shallow    │ ← sample slot 1
├─────────────┤
│     Mid      │ ← sample slot 2
├─────────────┤
│    Deep      │ ← sample slot 3
└─────────────┘
```

**Completion:** All depth slots filled → full geological model → maximum survey progress
**Correlation:** Adjacent completed columns connected with lines showing how layers extend → network bonus

**Integration point:** Phase 3 visualization
**Schedule:** Secondary — implement after core pipeline works

---

## Part 4: Recommended Integrated Design

Combine all variants into the pipeline described in [prospecting-master-design.md](prospecting-master-design.md):

| Pipeline Stage | Mechanics Used | Source |
|---------------|---------------|--------|
| Phase 0: Sweep | GPR radar with frequency slider | 7C |
| Phase 1: Sampling | Core drilling at selected depth | Base 7 + depth system |
| Phase 2: Sample Tray | Inventory + processing pipeline design | Base 7 + 7B |
| Phase 2→3: Lab Bench | Crush → separate → assay chain | 7B |
| Phase 3: Testing | Multi-tool analysis (XRF/LIBS/Fire Assay) | 7D |
| Phase 3: Output | Pathfinder tips in message bar | 7A (future) |
| Phase 3: Visualization | Stratigraphic column + cross-section | 7E (secondary) |
| Phase 3: Bonus | Cross-referencing adjacent cells | 7D |
| Ongoing: Clue chaining | Trail bonus from following pathfinders | 7A (future) |

### Implementation Priority Order
1. **Core pipeline:** Sweep → Sample → Tray → Test (7C + Base 7 + 7B + 7D)
2. **Depth system:** Stratigraphic columns and depth-dependent resources (7E)
3. **Spatial mechanics:** Pathfinder, clue chaining, cross-referencing (7A + 7D cross-ref)
