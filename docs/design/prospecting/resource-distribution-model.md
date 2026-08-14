# Resource Distribution Model

> Status: DRAFT (Implementation-Ready for Layers 1-3)
> Last Updated: 2026-04-30
> Parent: [prospecting-master-design.md](prospecting-master-design.md)
> Schedule: **Required for core prospecting pipeline.** Pathfinder/clue chaining deferred to future phase.

---

## Purpose

Define how resources are distributed across the planet with geological coherence at three scales:
1. **Geological provinces** (planet scale) — fixed layout, determines terrain type and baseline composition
2. **Deposit clusters** (planet scale, within provinces) — randomized per game, creates tactical variety
3. **Sub-cell distribution** (within one planet cell) — revealed by prospecting at NxN resolution

This replaces the current cluster-based generation in `ResourceManager` with a geologically coherent system that creates natural industry-favoring zones.

---

## Design Decisions

| Question | Decision |
|----------|----------|
| Province generation | **Voronoi regions** from fixed seed positions |
| Province layout | **Fixed** — mare near equator, polar at edges, highland flanking mare. Consistent strategic planning. |
| Deposit clusters | **Randomized** per planet seed. Different each playthrough. |
| Sub-cell algorithm | **Perlin noise** per resource group with correlation seeds |
| Minimum guarantees | **Regolith baseline** — every cell has trace amounts of common elements |
| Replay variety | **Fixed provinces, random deposits** — macro-strategy is learnable, micro-tactics vary |

---

## Layer 1: Geological Provinces

### Overview

The 20x20 planet grid is divided into **geological provinces** — large contiguous regions with distinct baseline resource compositions. Provinces are generated via Voronoi tessellation from fixed seed points.

The grid represents a lunar surface in pseudo-Mercator projection:
- **Y axis** = latitude (Y=0 and Y=19 are polar, Y=9-10 is equatorial)
- **X axis** = longitude

### Province Types

Five province types based on lunar geology:

| Province | Target Coverage | Geological Basis | Visual |
|----------|----------------|-----------------|--------|
| **Mare Basalt** | ~25% (100 cells) | Dark basaltic lava plains, iron/titanium-rich | Dark gray terrain |
| **Highland Anorthosite** | ~35% (140 cells) | Bright feldspathic crust, aluminum/calcium-rich | Light gray terrain |
| **KREEP Terrain** | ~10% (40 cells) | Potassium, Rare Earth Elements, Phosphorus concentration | Reddish-brown terrain |
| **Polar Volatile** | ~15% (60 cells) | Permanently shadowed regions, volatile-trapping cold zones | Blue-white terrain |
| **Transition Zone** | ~15% (60 cells) | Blended boundaries between provinces | Mixed coloring |

### Fixed Voronoi Seed Layout

12 seed points at fixed relative positions (scaled to grid size). These never change between playthroughs.

```
Y=0  ┌──────────────────────────────────────┐
     │  P1          P2          P3          │  Polar Volatile (3 seeds)
Y=3  │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │
     │      H1              H2              │  Highland (2 seeds)
Y=7  │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │
     │  M1        K1    M2        M3        │  Mare (3) + KREEP (1)
Y=13 │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │
     │          H3              H4          │  Highland (2 seeds)
Y=17 │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │
     │      P4              P5              │  Polar Volatile (2 seeds)
Y=19 └──────────────────────────────────────┘
     X=0                                X=19
```

**Seed positions (grid coordinates):**

| Seed | Province Type | Position (X, Y) |
|------|--------------|-----------------|
| P1 | Polar Volatile | (3, 1) |
| P2 | Polar Volatile | (10, 0) |
| P3 | Polar Volatile | (17, 1) |
| H1 | Highland | (5, 5) |
| H2 | Highland | (15, 4) |
| M1 | Mare Basalt | (2, 10) |
| K1 | KREEP Terrain | (8, 9) |
| M2 | Mare Basalt | (13, 10) |
| M3 | Mare Basalt | (18, 11) |
| H3 | Highland | (7, 15) |
| H4 | Highland | (16, 16) |
| P4 | Polar Volatile | (4, 18) |
| P5 | Polar Volatile | (14, 19) |

**Assignment:** Each grid cell is assigned to the nearest Voronoi seed. Cells within distance ≤ 1.5 of a province boundary are classified as **Transition Zone** (blended composition from both neighbors).

### Province Baseline Compositions

Each province defines a baseline abundance profile. Values are relative (0.0-1.0 scale), multiplied by per-resource scaling factors at generation time.

| Resource | Mare | Highland | KREEP | Polar | Transition | Min Floor |
|----------|------|----------|-------|-------|------------|-----------|
| **Fe** | 0.60 | 0.12 | 0.30 | 0.08 | blend | 0.03 |
| **Ti** | 0.40 | 0.04 | 0.15 | 0.02 | blend | 0.00 |
| **Si** | 0.25 | 0.50 | 0.30 | 0.15 | blend | 0.05 |
| **Al** | 0.10 | 0.50 | 0.20 | 0.10 | blend | 0.03 |
| **Ca** | 0.10 | 0.40 | 0.20 | 0.08 | blend | 0.03 |
| **H2** | 0.03 | 0.02 | 0.03 | 0.50 | blend | 0.01 |
| **O2** | 0.30 | 0.40 | 0.35 | 0.20 | blend | 0.05 |
| **C** | 0.02 | 0.01 | 0.02 | 0.15 | blend | 0.00 |

**Transition zone blending:** Weighted average of the two nearest provinces, based on relative distance to each Voronoi seed.

### Province Resource Scaling Factors

Province baselines (0.0-1.0) are multiplied by per-resource scaling factors to produce actual game abundance values. These are calibrated against extraction rates and production costs.

| Resource | Scale Factor | Max Possible | Rationale |
|----------|-------------|-------------|-----------|
| Fe | 6000.0 | ~3600 (mare) | Primary structural material |
| Ti | 4000.0 | ~1600 (mare) | Industrial specialty, less common |
| Si | 2000.0 | ~1000 (highland) | Moderate abundance, many uses |
| Al | 3500.0 | ~1750 (highland) | Construction primary |
| Ca | 2500.0 | ~1000 (highland) | Construction secondary |
| H2 | 5000.0 | ~2500 (polar) | Critical volatile, high value |
| O2 | 4000.0 | ~1600 (highland) | Ubiquitous but needed in quantity |
| C | 3000.0 | ~450 (polar) | Rare outside polar regions |

These match the current system's scale factors. Province baselines replace the old cluster-center generation.

---

## Layer 2: Deposit Clusters

### Overview

On top of province baselines, **deposit clusters** add localized enrichment zones. These are randomized per planet seed, creating tactical variety within the fixed province layout. Deposits represent ore bodies, ice pockets, ilmenite lenses, and other geological features.

### Cluster Generation Rules

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Clusters per province cell | 0-2 (random) | Sparse enough to create exploration incentive |
| Cluster radius | 1-3 cells | Smaller than provinces, multiple cells |
| Enrichment multiplier | 1.5x - 3.0x baseline | Noticeable but not overwhelming |
| Province-appropriate probability | 80% | Most clusters match province geology |
| Cross-province surprise probability | 20% | Rare surprises (water in mare, Ti in highland) |

### Province-Appropriate Deposit Types

| Province | Common Deposits | Enrichment | Gameplay Reason |
|----------|----------------|-----------|-----------------|
| **Mare** | Fe vein, Ti (ilmenite) lens | Fe to 0.9, Ti to 0.7 | Rich manufacturing zones |
| **Mare** | Olivine pocket (Si+Fe) | Si to 0.5, Fe to 0.8 | Multi-resource deposits |
| **Highland** | Anorthosite body (Al+Ca) | Al to 0.8, Ca to 0.7 | Prime construction material |
| **Highland** | Si-rich regolith pocket | Si to 0.8 | Glass/electronics feedstock |
| **KREEP** | Thorium hotspot | Th×2 (survey bonus) | Science boost |
| **KREEP** | Mixed enrichment | All metals +30% | Jack-of-all-trades zone |
| **Polar** | Ice deposit (H2O lens) | H2 to 0.8, C to 0.3 | Critical water source |
| **Polar** | Cold-trap volatile cache | H2 to 0.7, C to 0.25 | Farming/life support |

### Cross-Province Surprises (20% chance per cluster)

These reward thorough exploration and create strategic decisions:

| Surprise | Where Found | Enrichment | Strategic Value |
|----------|-------------|-----------|-----------------|
| Fracture ice | Mare, Highland | H2 to 0.3 in fractured bedrock | Water without going polar |
| Impact Ti | Highland | Ti to 0.3 | Manufacturing in highland |
| KREEP fragment | Mare, Highland | Th/K boost (survey) | Science opportunity |
| Buried volatiles | KREEP | H2 to 0.2, C to 0.1 | Life support in unexpected location |

### Cluster Generation Algorithm

```
For each province:
    For each cell in province:
        roll = random(0.0, 1.0)
        if roll < CLUSTER_PROBABILITY (0.15):
            type = select_cluster_type(province, roll2)
            radius = random(1.0, 3.0)
            enrichment = random(1.5, 3.0) * baseline
            apply enrichment to cell and neighbors within radius
            (linear falloff from center to edge)
```

### Guaranteed Deposits

To ensure every game is playable and has meaningful strategic variety, the following deposits are **guaranteed** (placed deterministically, then random deposits added on top):

| Guarantee | Province | Count | Rationale |
|-----------|----------|-------|-----------|
| At least 1 rich Fe deposit (≥0.8) | Mare | 1 | Manufacturing must be viable |
| At least 1 rich Al+Ca deposit (≥0.7 each) | Highland | 1 | Construction must be viable |
| At least 1 ice deposit (H2≥0.7) | Polar | 1 | Water/farming must be viable |
| At least 1 KREEP hotspot | KREEP | 1 | Science must be viable |
| At least 1 cross-province water | Mare or Highland | 1 | Alternative water strategy |

---

## Layer 3: Sub-Cell Distribution

### Overview

When a prospecting unit first operates on a planet cell, the cell's resources are distributed across an NxN sub-cell grid. This is what the player explores during the prospecting pipeline.

### Algorithm: Correlated Perlin Noise

Resources are organized into **correlation groups** — resources that naturally co-occur in the same geological formations. Resources within a group share the same base Perlin noise seed, creating deposits where they cluster together.

#### Correlation Groups

| Group | Resources | Shared Seed | Geological Basis |
|-------|-----------|-------------|------------------|
| **Mafic** | Fe, Ti | Seed A | Co-occur in ilmenite (FeTiO3), pyroxene |
| **Felsic** | Si, Al, Ca | Seed B | Co-occur in plagioclase feldspar, anorthosite |
| **Volatile** | H2, C | Seed C | Co-occur in cold traps and regolith pockets |
| **Oxygen** | O2 | Seed D (independent) | Present in all minerals, independent variation |

#### Anti-Correlation

Mafic and Felsic groups are **anti-correlated**: Seed B = inverted Seed A. Where Fe+Ti concentrate, Si+Al+Ca are depleted, and vice versa. This matches real geology — mare basalt (Fe-rich) and anorthosite (Al-rich) are fundamentally different rock types.

```
noise_mafic[x][y]  = perlin(x, y, seedA)        // range [-1, +1]
noise_felsic[x][y] = -noise_mafic[x][y]         // inverted
noise_volatile[x][y] = perlin(x, y, seedC)       // independent
noise_oxygen[x][y] = perlin(x, y, seedD)         // independent
```

#### Generation Formula

For each resource in a sub-cell:

```
raw_value = cell_abundance + noise[x][y] * spread * cell_abundance

Where:
  cell_abundance = planet cell's resource value (from Layer 1 + Layer 2)
  noise[x][y]    = Perlin value for this resource's correlation group
  spread         = variation factor (how much sub-cells differ from average)
```

**Spread factor** controls intra-cell variation:

| Province Type | Spread | Effect |
|--------------|--------|--------|
| Mare | 0.6 | Moderate variation — lava flows are relatively uniform |
| Highland | 0.8 | High variation — impact mixing creates patchwork |
| KREEP | 0.5 | Lower variation — KREEP is diffuse enrichment |
| Polar | 0.9 | High variation — ice deposits are very localized (PSRs) |

#### Normalization

After generating all sub-cell values, normalize so the average equals the planet cell's abundance:

```
actual_avg = mean(all_subcells)
correction = cell_abundance / actual_avg
subcell[x][y] *= correction
subcell[x][y] = clamp(subcell[x][y], 0.0, cell_abundance * 3.0)
```

Upper clamp at 3x prevents extreme outliers while allowing hot spots. Floor at 0.0 allows genuinely empty sub-cells.

### Grid Size and Resolution

The sub-cell grid scales with prospecting tier (see [prospecting-master-design.md](prospecting-master-design.md)):

| Tier | Grid Size | Sub-Cells | Noise Frequency | Effect |
|------|-----------|-----------|-----------------|--------|
| T0 | 3x3 | 9 | Low (1 octave) | Broad zones — "this corner is Fe-rich" |
| T1 | 4x4 | 16 | Low-medium | Individual deposits visible |
| T2 | 5x5 | 25 | Medium | Deposit boundaries clear |
| T3 | 6x6 | 36 | Medium-high (2 octaves) | Fine detail — precise deposit mapping |

**Tier upgrade re-generation:** When the grid expands at tier-up, the Perlin noise is sampled at higher resolution over the same spatial area. Because Perlin noise is continuous, the coarser grid is a valid low-resolution view of the finer grid — existing survey data remains consistent.

### Seed Construction

Sub-cell noise seeds are deterministic, constructed from:

```
seedA = hash(planetSeed, cellX, cellY, GROUP_MAFIC)
seedB = seedA (inverted in usage)
seedC = hash(planetSeed, cellX, cellY, GROUP_VOLATILE)
seedD = hash(planetSeed, cellX, cellY, GROUP_OXYGEN)
```

Same planet seed + same cell = same sub-cell distribution, regardless of when prospecting begins.

### Depth Layer Interaction

Sub-cell values are the **surface layer** baseline. Depth layers apply the existing depth bias multipliers per sub-cell:

```
subcell_depth[x][y][layer] = subcell_surface[x][y] * depth_bias[resource][layer]
```

Existing depth biases (from resource_manager.cpp):

| Resource | Surface (L0) | Shallow (L1) | Mid (L2) | Deep (L3) |
|----------|-------------|-------------|---------|----------|
| H2 | 1.5x | 1.0x | 0.5x | 0.3x |
| O2 | 1.3x | 1.0x | 0.8x | 0.6x |
| C | 1.4x | 1.0x | 0.7x | 0.5x |
| Fe | 0.6x | 1.0x | 0.9x | 1.8x |
| Si | 0.8x | 1.0x | 1.3x | 0.7x |
| Ti | 0.4x | 1.0x | 1.0x | 2.0x |
| Al | 0.8x | 1.0x | 1.2x | 0.9x |
| Ca | 0.9x | 1.0x | 1.3x | 0.8x |

This means a sub-cell that's Fe-rich at the surface is *even more* Fe-rich at depth (0.6x → 1.8x = 3x increase). Deep prospecting in the right sub-cell is very rewarding.

---

## Minimum Guarantees

### Regolith Baseline (Per Cell)

Every planet cell has a minimum resource floor, regardless of province. The Moon's surface has been impact-gardened for 4 billion years — regolith everywhere contains trace amounts of common elements.

| Resource | Minimum Floor | Rationale |
|----------|--------------|-----------|
| Fe | 0.03 | Pyroxene/olivine ubiquitous in regolith |
| Ti | 0.00 | Genuinely absent in pure anorthosite |
| Si | 0.05 | 20-25% SiO2 in all lunar regolith |
| Al | 0.03 | Plagioclase feldspar everywhere |
| Ca | 0.03 | Always present in pyroxene and feldspar |
| H2 | 0.01 | Solar wind implantation (trace everywhere) |
| O2 | 0.05 | ~45% of regolith mass is oxygen |
| C | 0.00 | Genuinely rare outside polar/volcanic |

**Applied at:** Planet cell level, after province baseline + deposit clusters. If any cell falls below the floor, it's raised to the floor value.

### Game-Level Guarantees

The fixed province layout ensures every game has:

| Guarantee | How Ensured |
|-----------|-------------|
| Manufacturing-viable zone exists | Mare provinces always at equator (3 seeds) |
| Construction-viable zone exists | Highland provinces always flanking mare (4 seeds) |
| Water/farming-viable zone exists | Polar provinces always at grid edges (5 seeds) |
| Science-viable zone exists | KREEP province always near mare/highland boundary (1 seed) |
| Alternative water source exists | Guaranteed cross-province ice deposit in mare or highland |
| No completely barren cells | Regolith baseline floor on all cells |

### Colony Viability Anywhere

A colony placed in any province can survive (not thrive) because:
- **Energy:** Solar illumination available everywhere (better at equator, but nonzero at poles)
- **Oxygen:** O2 floor of 0.05 everywhere — enough for basic extraction
- **Construction basics:** Si=0.05, Al=0.03, Ca=0.03, Fe=0.03 everywhere — slow but possible
- **Water:** H2 floor of 0.01 — survival extraction possible, but the player should seek better sources

**Thriving** requires choosing the right province for your primary industry. The design creates clear strategic choices without making any location completely unviable.

---

## Industry Zone Design

### Why Clusters Create Strategic Decisions

The province system naturally creates preferred zones for each unit type:

| Unit Type | Primary Need | Best Province | Why |
|-----------|-------------|--------------|-----|
| **Extraction** | Abundant raw materials | Depends on target | Goes where the ore is |
| **Farming** | H2O, C, solar energy | Polar (water) but equatorial (solar) | Tension: water vs. energy |
| **Energy** | Solar illumination, H2 (fuel cells) | Equatorial (solar) or Polar (H2) | Two viable strategies |
| **Manufacturing** | Fe, Ti, Si, energy | Mare (metals) | Clear industrial zone |
| **Research** | KREEP elements, sample diversity | KREEP or boundaries | Benefits from variety |
| **Construction** | Al, Ca, Si | Highland | Aluminum + calcium-rich |

### Strategic Tensions

The distribution creates several deliberate tensions:

1. **Farming dilemma:** Water is polar, solar energy is equatorial. Player must either:
   - Farm at poles (abundant water, poor solar → needs H2 fuel cells for energy)
   - Farm at equator (great solar, poor water → needs water transport from polar)
   - Find a cross-province water surprise (rare, rewards exploration)

2. **Manufacturing vs. construction:** Mare has metals (Fe+Ti), highland has construction materials (Al+Ca+Si). A colony can't easily optimize for both — trade between colonies or accept a suboptimal location.

3. **KREEP gamble:** KREEP terrain has moderate amounts of everything but excels at nothing. Good for research but depends on trade for industrial output.

4. **Colony placement strategy:** The site selection view (existing) gives orbital survey data. The province system makes this data more meaningful — the player is really choosing which industry to optimize for.

### Multi-Colony Strategy

The province layout encourages specialization across colonies:

```
Colony A (Polar)          Colony B (Mare)           Colony C (Highland)
├─ Farming hub            ├─ Manufacturing hub      ├─ Construction hub
├─ Water export           ├─ Metal export           ├─ Materials export
├─ Energy: fuel cells     ├─ Energy: solar          ├─ Energy: solar
└─ Imports: metals        └─ Imports: water, Al     └─ Imports: water, Fe
```

---

## Interaction with Existing Systems

### OrbitalSurveyData (Unchanged)

`OrbitalSurveyData` is still generated from planet cell abundances. The province system changes what those abundances ARE, but the survey data generation algorithm is unchanged. Province baselines + deposit clusters produce the cell abundance values → survey data normalizes them into percentages.

### SiteArchetype (Naturally Aligned)

The existing `GetSiteArchetype()` classification (MARE_INDUSTRIAL, HIGHLAND_CONSTRUCTION, POLAR_VOLATILE, KREEP_SCIENTIFIC, LAVA_TUBE, MIXED) now naturally aligns with province types:

| Province | Expected Archetype | Notes |
|----------|-------------------|-------|
| Mare | MARE_INDUSTRIAL | Strong Fe+Ti scores → mare archetype |
| Highland | HIGHLAND_CONSTRUCTION | Strong Si+Al+Ca scores → highland archetype |
| KREEP | KREEP_SCIENTIFIC | Strong Th/K → KREEP archetype |
| Polar | POLAR_VOLATILE | Strong H2 signal → polar archetype |
| Transition | MIXED or neighbor archetype | Blended scores → mixed or weak match |

The archetype system doesn't need to change — it correctly classifies provinces after the fact.

### Sect Placement Preview (Unchanged)

The Ctrl+hover preview in Colony view shows LOW/MED/HIGH resource categories. These are derived from cell abundances, which are now province-determined. No code change needed.

### ProcessExtraction Pipeline (Unchanged)

Extraction reads `surveyProgress` and `markedSites` from prospecting output. The new sub-cell distribution feeds into prospecting's analysis tools (which produce survey progress), but the extraction pipeline formula doesn't change.

---

## Resource Map Initialization

### Generation Order

```
1. Fixed Voronoi seed placement (always the same)
2. Province assignment per cell (nearest seed, transition zone check)
3. Province baseline composition per cell
4. Guaranteed deposits placed (deterministic positions within provinces)
5. Random deposit clusters added (seeded from planetSeed)
6. Minimum floors applied
7. OrbitalSurveyData generated from final cell values (existing code)
8. SiteArchetype classified from survey data (existing code)
```

Steps 1-6 replace the current `GenerateResources()` cluster loop. Steps 7-8 are unchanged.

### Sub-Cell Generation (On-Demand)

Sub-cell grids are NOT generated at planet creation. They are generated **on-demand** when a prospecting unit first operates on a cell:

```
9. Player starts prospecting in cell (gX, gY)
10. Generate sub-cell Perlin noise for each correlation group
11. Distribute cell abundance across NxN sub-cells
12. Apply depth bias per sub-cell per layer
13. Cache result (deterministic, so only computed once per cell)
```

This avoids generating sub-cell data for all 400 planet cells up front. Only cells that are actually prospected need sub-cell resolution.

---

## Pathfinder Correlation Rules (Deferred)

> Deferred to future development phase. Requires geological coherence model across adjacent cells.

Lunar-appropriate pathfinder correlations for future implementation:

| Indicator | Target | Geological Basis |
|-----------|--------|------------------|
| High Ti surface → | Fe concentration at depth | Ilmenite (FeTiO3) — Ti indicates iron-bearing mineral |
| Th anomaly → | Rare earth elements nearby | KREEP terrain geochemistry |
| H2 surface → | H2O at fractured bedrock depth | Solar wind H2 migrates to cold traps |
| High Ca + low Fe → | Al-rich anorthosite body | Pure plagioclase indicator |
| Si gradient → | Deposit boundary (mare/highland contact) | Composition change indicates terrain transition |

### Clue Chaining (Deferred)

Deposits generated by the province + cluster system already form multi-cell bodies (cluster radius 1-3). Following a gradient uphill within a deposit naturally leads to the richest point — this is the basis for future clue chaining without additional generation work.

---

## Open Questions (Remaining)

| Question | Status |
|----------|--------|
| Perlin noise octave count at each tier | Implementation detail — prototype and tune |
| Exact Perlin frequency scaling factor | Implementation detail — start with 1.0/gridSize |
| Should deposit clusters affect depth distribution differently from surface? | Deferred — current depth bias is uniform |
| LAVA_TUBE archetype — should this be a 6th province type? | Deferred — currently a special archetype classification |
| Geological events (impacts, volcanism) as map modifiers | Future feature |
| Per-resource scaling factor tuning | Needs balance pass after implementation |
