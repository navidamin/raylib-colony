# Prospecting UI Layout

> Status: DRAFT
> Last Updated: 2026-04-02
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define the visual layout and interaction flow for the prospecting module's menu, including stage views, sample visualization, and information display.

## Initial View Concept

When the player opens the prospecting menu, they see:

```
┌─────────────────────────────────────────────────────────────┐
│  PROSPECTING                           [Default ▼] [AI ◉]  │
├────────────┬────────────┬──────────────┬────────────────────┤
│  ◉ Sweep   │  ○ Samples │  ○ Lab       │  Stage tabs        │
├────────────┴────────────┴──────────────┴────────────────────┤
│                                                             │
│  ┌─ Stage Content Area ────────────────────────────────┐   │
│  │                                                      │   │
│  │  [Content changes based on selected stage tab]       │   │
│  │                                                      │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ Sample Overview Bar ───────────────────────────────┐   │
│  │  [Sample icons with visual encoding — see below]     │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ Message Bar ───────────────────────────────────────┐   │
│  │  Pathfinder tips / Status messages / Alerts          │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ ▶ Objectives (2/5) ───────────────────────────────┐   │
│  │  [Collapsible — click to expand/collapse]            │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Stage-Specific Views

### Sweep View (Phase 0)
- 5x5 grid with confidence heat map overlay (color-coded: red→green)
- **Continuous frequency slider** (shallow-detailed ↔ deep-blurry)
- Sweep button + energy cost display
- Results summary panel
- One sweep per frequency band allowed (no same-frequency repeats, but multiple frequencies OK)

### Samples View (Phase 1+2)
- Grid for cell selection (sampling target)
- Depth layer selector (Regolith / Megaregolith / Fractured Bedrock / Intact Bedrock)
- Sample tray inventory (4/8/12/16 capacity by tier)
- Initial data display per sample
- **Discard button** per sample (free, anytime)
- Pipeline design interface (tool assignment per sample)

### Lab View (Phase 3)
- **Pipeline preset buttons** at top (see Pipeline Presets below)
- Custom pipeline option
- Processing queue / batch slots
- Active processing animation
- Incoming results data stream
- Completed results archive
- Stratigraphic column display (7E)

**Processing times:** Near-instant for all tools except fire assay (which has a meaningful wait period).

**Multi-tool:** Sequential tools can be applied to the same sample. Fire assay ends the chain (destructive — consumes sample).

## Pipeline Presets

Four preset buttons in the Lab view for common analysis workflows:

| Preset | Target Elements | Tools Used | Use Case |
|--------|----------------|-----------|----------|
| **Structural** | Fe, Si, Ti | Magnetic separation + XRF | Building materials, manufacturing sects |
| **Life Support** | H₂O, O₂, H₂ | Volatile extraction + LIBS | Survival resources, polar sites |
| **Strategic** | Au, Pt, Li, He-3 | Heavy mineral separation + Fire Assay | High-value elements, late-game |
| **Full Survey** | All elements | All separations + all tools | Maximum data, highest cost |

Presets auto-assign the specified pipeline to selected samples. Player can always override with custom tool assignment.

## Sample Visual Design (Revised — 3D Crystal Approach)

### Concept

Each sample is rendered as a **3D transparent multi-faceted crystal shape**, pre-rendered to sprite textures. Samples are not flat icons — they read as small translucent mineral specimens that encode gameplay data through shape, color, size, and glow.

### Pre-Rendering Pipeline

Base shapes are **hand-modeled 3D meshes** (in Blender or equivalent), exported and rendered offline to sprite sheets:

1. Model 20 base crystal meshes (4 families × 5 shapes)
2. Render each at multiple sizes (for richness encoding) and multiple glow levels (for confidence)
3. Export as sprite atlas with transparent backgrounds
4. Game loads sprites and selects the correct variant at runtime based on sample data

This avoids real-time 3D rendering per-sample. The game draws 2D sprites — fast and simple.

**Sprite atlas organization:**

```
sprites/samples/
    family_a/          (Angular Chunks — surface regolith)
        a1_cleaved/
            size_1_glow_0.png    (small, no glow = poor + uncertain)
            size_1_glow_1.png
            ...
            size_4_glow_4.png    (large, bright glow = rich + certain)
        a2_shatter/
            ...
    family_b/          (Crystalline Shards — fractured/intact bedrock)
    family_c/          (Rounded Nodules — megaregolith)
    family_d/          (Layered Slabs — sedimentary)
```

**Render parameters per sprite:**
- Resolution: 64×64px per sample icon (scales to screen via FS())
- Background: fully transparent (alpha=0)
- Lighting: single directional light, slight rim highlight for depth
- Material: translucent glass-like shader, colored by element tint
- Camera angle: consistent 3/4 isometric view across all shapes

### Visual Encoding

| Gameplay Data | Visual Channel | Detail |
|---------------|---------------|--------|
| **Element composition** | Color / tint | Crystal body color matches dominant element (see palette below) |
| **Confidence level** | Glow / emission | Edge glow intensity: dull (Very Low) → bright halo (Certain) |
| **Depth layer** | Shape family | Surface=rough chunks, deep=geometric crystals (see family mapping) |
| **Richness** | Size | Poor deposit=small sprite, rich deposit=large sprite |
| **Processing state** | Animation | Static=in tray, slow rotate=processing, bright flash=analysis complete |

### Element Color Palette

| Element | Color | Hex (approximate) | Rationale |
|---------|-------|--------------------|-----------|
| Fe (Iron) | Warm red-brown | #B5463C | Rust/hematite |
| Ti (Titanium) | Cool silver-blue | #A0B0C0 | Metallic titanium |
| Si (Silicon) | Pale amber | #D4A850 | Sandy silicate |
| Al (Aluminium) | Light gray | #C0C0C8 | Brushed aluminium |
| Ca (Calcium) | Off-white cream | #E8DCC0 | Limestone/chalk |
| H2O (Water) | Clear blue | #4488CC | Water ice |
| H2 (Hydrogen) | Pale cyan | #88CCEE | Light gas |
| O2 (Oxygen) | Soft teal | #55AA99 | — |
| C (Carbon) | Dark charcoal | #404040 | Graphite/coal |
| He-3 | Violet purple | #8866BB | Rare/exotic signal |

**Multi-element samples:** When a sample contains multiple significant elements, the crystal uses the dominant element's color as the base tint, with secondary element visible as internal inclusions or color bands (modeled into the appropriate shape variants).

### Shape Families — Depth Mapping

Each depth layer maps to a preferred shape family, reflecting geological character:

| Depth Layer | Primary Family | Visual Character | Why |
|-------------|---------------|-----------------|-----|
| **Regolith** (surface) | A: Angular Chunks | Rough, fractured, irregular | Impact-shattered surface rubble |
| **Megaregolith** | C: Rounded Nodules | Smooth, weathered, organic | Pressure-compacted, tumbled fragments |
| **Fractured Bedrock** | D: Layered Slabs | Flat, stacked, sedimentary | Sheared geological layers |
| **Intact Bedrock** | B: Crystalline Shards | Sharp, geometric, prismatic | Pristine mineral formations |

Selection: **70% from primary family, 30% random** (same rule as before, but families now encode depth instead of being purely decorative).

### Shape Templates (20 base meshes)

Each mesh is a hand-modeled 3D crystal/rock with transparent material. Fragment counts indicate visual complexity, not polygon count.

#### Family A: Angular Chunks (Regolith — surface debris)

| # | Name | Visual Description |
|---|------|--------------------|
| A1 | Cleaved Block | Two large rectangular halves with offset fracture, exposed interior faces catch light |
| A2 | Shatter | Radial fragments from center impact, sharp edges, visible through gaps |
| A3 | Wedge Pair | Two opposing triangular wedges with translucent gap between them |
| A4 | Stacked Fracture | Horizontal layers with jagged breaks, light passes through cracks |
| A5 | Corner Break | Large block with one corner chipped off, interior facet visible |

#### Family B: Crystalline Shards (Intact Bedrock — deep pristine minerals)

| # | Name | Visual Description |
|---|------|--------------------|
| B1 | Single Crystal | Tall hexagonal prism, internal light refraction, small chip fragments at base |
| B2 | Twin Growth | Two intersecting elongated hexagons, overlapping transparency creates darker zones |
| B3 | Needle Cluster | Thin pointed crystals radiating from center, light catches each edge |
| B4 | Tabular | Wide flat hexagonal plate, thin enough to be semi-transparent, prismatic edge glints |
| B5 | Druzy | Flat base covered in small pointed crystal growth, sparkling surface |

#### Family C: Rounded Nodules (Megaregolith — compacted rubble)

| # | Name | Visual Description |
|---|------|--------------------|
| C1 | Cobble | One large smooth rounded stone, translucent edges, small chips nearby |
| C2 | Botryoidal | Cluster of overlapping rounded forms (grape-like), light pools in crevices |
| C3 | Concretion | Large oval with crescent shell fragments, internal layers visible through shell |
| C4 | Pebble Scatter | Loose cluster of small rounded stones, varied transparency per pebble |
| C5 | Split Nodule | Oval split in half, crystalline interior exposed, geode-like |

#### Family D: Layered Slabs (Fractured Bedrock — sheared layers)

| # | Name | Visual Description |
|---|------|--------------------|
| D1 | Flagstone | Thin wide rectangles stacked with offsets, light passes between layers |
| D2 | Shale Split | Very thin irregular slabs fanning apart, translucent at edges |
| D3 | Cross-Bedded | Diagonal slabs crossed by perpendicular pieces, grid-like light pattern |
| D4 | Laminate | Tight stack of horizontal strips, banding effect through transparency |
| D5 | Breccia Slab | Flat slab with angular fragment inclusions embedded inside, visible through surface |

### Glow / Confidence Rendering

Glow is rendered into the sprite at pre-render time (not a runtime post-process):

| Confidence Level | Glow Intensity | Visual |
|-----------------|----------------|--------|
| Very Low (0-20%) | 0 — no glow | Crystal looks dull, matte surface, minimal light interaction |
| Low (21-40%) | 1 — faint edge highlight | Subtle rim light on facet edges |
| Moderate (41-60%) | 2 — soft glow | Visible halo, facets begin to catch light |
| High (61-80%) | 3 — bright glow | Strong edge emission, internal light visible |
| Certain (81-100%) | 4 — full emission | Crystal radiates light, bright halo, facets fully lit |

**5 glow levels × 4 sizes × 20 templates = 400 sprite variants.** At 64×64px each this is ~1.6 MB uncompressed — easily fits in a single texture atlas.

### Size / Richness Scaling

| Richness | Size Level | Sprite Dimensions | Visual |
|----------|-----------|-------------------|--------|
| Poor (<25%) | 1 — Small | 40×40px within 64px cell | Barely fills the tray slot |
| Below Average (25-50%) | 2 — Medium | 48×48px | Standard size |
| Above Average (50-75%) | 3 — Large | 56×56px | Noticeable presence |
| Rich (>75%) | 4 — Full | 64×64px | Fills the slot, immediately eye-catching |

### Processing Animation

Animations are runtime sprite manipulation, not pre-rendered:

| State | Animation | Implementation |
|-------|-----------|---------------|
| In tray | Static | No transform |
| Processing | Slow rotation | Rotate sprite 30°/sec around center |
| Analysis complete | Bright flash + settle | White additive flash (0.3s), then static with final glow level |

### Sample Tray Display

The sample tray shows all current samples in a horizontal bar:

```
┌─ Sample Tray (6/8) ─────────────────────────────────────────────┐
│                                                                   │
│   [◆]  [◇]  [◆]  [◆]  [◇]  [◆]  [  ]  [  ]                   │
│    Fe   Ti   H2O   Fe   Si   Ca                                  │
│                                                                   │
│   ◆ = sample present (3D crystal sprite)                         │
│   ◇ = processing (rotating)                                      │
│   [  ] = empty slot                                              │
└───────────────────────────────────────────────────────────────────┘
```

- All samples visible simultaneously (tray capacity: 4/8/12/16 by tier)
- Selected sample highlighted with a border or subtle scale-up
- Hovering a sample shows a tooltip with element breakdown and confidence
- Element label below each sample in small text (dominant element abbreviation)

### Implementation Notes

**Asset creation workflow:**
1. Model 20 base meshes in Blender (low-poly, ~200-500 tris each)
2. Apply glass/translucent material with element color as a parameter
3. Set up render rig: fixed camera, directional light, transparent background
4. Batch render: 20 shapes × 10 colors × 5 glow levels × 4 sizes = 4,000 sprites
5. Pack into texture atlas (or use color tinting at runtime to reduce to 20 × 5 × 4 = 400 base sprites, tinted per-element)

**Runtime color tinting (recommended optimization):**
Rather than pre-rendering every element color, render shapes as neutral gray/white crystal and apply element color as a tint multiply at runtime via raylib's `DrawTexturePro()` tint parameter. This reduces the atlas to **20 shapes × 5 glow levels × 4 sizes = 400 sprites** and allows easy addition of new elements without re-rendering.

**Sprite atlas sizing (with runtime tinting):**
- 400 sprites × 64×64px = ~1.6 million pixels
- Fits in a single 2048×2048 texture atlas (1024 slots available)
- Memory: ~16 MB RGBA uncompressed, ~4 MB compressed

## Objectives Display

### Objectives Panel (Bottom Collapsible)

Small collapsible section at the bottom of the prospecting panel, below the message bar. Click the header to expand/collapse. Collapsed by default to keep the panel uncluttered; expanded shows active objectives.

```
┌─ ▶ Objectives (2/5) ─────────────────────────────────┐  ← collapsed (click to expand)
└───────────────────────────────────────────────────────┘

┌─ ▼ Objectives (2/5) ─────────────────────────────────┐  ← expanded
│  ☑ Collect your first core sample              [T0]  │
│  ☑ Visually inspect a sample                   [T0]  │
│  ☐ Collect samples from 3 different cells      [T0]  │
│  ☐ Fill your sample tray (4/4)          → +1 slot    │
│                                                       │
│  ☐ Run your first GPR surface sweep            [T1]  │  ← appears after T1 unlock
└───────────────────────────────────────────────────────┘
```

- **Collapsed header** shows completed/total count (e.g., "2/5")
- Objectives are **tier-locked** — new objectives appear when the tier unlocks
- **No deadlines** — objectives persist indefinitely until completed
- **Rewards** shown inline (e.g., "→ +1 slot", "→ Structural preset")
- Completed objectives show checkmark and are dimmed
- Tutorial objectives (no reward) show no reward label

See [prospecting-master-design.md](prospecting-master-design.md) Section 11 for the full 18-objective list across T0-T3.

## Interaction Flow

```
Player opens Prospecting menu
    │
    ├─► Sweep tab: Configure frequency slider, run GPR sweeps
    │       └─► Results update grid heat map (confidence colors)
    │
    ├─► Samples tab: Select cells, choose depths, collect
    │       ├─► New sample appears in tray (procedural ore icon)
    │       ├─► Discard unwanted samples (free)
    │       └─► Assign processing pipeline or select preset
    │
    └─► Lab tab: Select preset or custom pipeline, monitor processing
            ├─► Sequential tools applied to each sample
            ├─► Fire assay ends chain (destructive)
            ├─► Completed samples show element data
            ├─► Pathfinder tips appear in message bar
            └─► Survey progress updates per cell
```

## Stratigraphy Side Panel

The stratigraphy view appears as a **side panel** next to the grid when the player hovers or selects a cell that has multiple depth samples. It does not occupy a separate tab.

```
┌─ Stage Content Area ──────────────────────────────────────────────┐
│                                                                    │
│  ┌─ Grid ────────────────────┐  ┌─ Stratigraphy Panel ─────────┐ │
│  │                            │  │                               │ │
│  │   [NxN grid with heat      │  │  Cell (2,3) — Depth Column   │ │
│  │    map / sample markers]   │  │  ┌─────────────────────────┐ │ │
│  │                            │  │  │ Regolith      Fe 42%    │ │ │
│  │                            │  │  │ ─────────────────────── │ │ │
│  │                            │  │  │ Megaregolith  Ti 18%    │ │ │
│  │                            │  │  │ ─────────────────────── │ │ │
│  │                            │  │  │ Fract.Bedrock H₂O 31%  │ │ │
│  │                            │  │  │ ─────────────────────── │ │ │
│  │                            │  │  │ Intact Bedr.  He-3 7%   │ │ │
│  │                            │  │  └─────────────────────────┘ │ │
│  │                            │  │                               │ │
│  │                            │  │  Adjacent correlation lines  │ │
│  │                            │  │  shown when neighbor columns │ │
│  │                            │  │  are also complete            │ │
│  └────────────────────────────┘  └───────────────────────────────┘ │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

**Trigger:** Hover or select a cell with ≥2 depth layer samples. Panel slides in from the right, compressing the grid slightly. If the cell has only surface samples, no panel appears.

**Content:**
- Vertical depth column for the selected cell
- Layer bars colored by dominant element, with composition percentages
- Unsampled layers shown as gray/hatched placeholder
- When adjacent cells also have complete columns, correlation lines connect matching layers across columns

## Font Scaling

The prospecting UI uses the **same FS() multiplier** as the existing extraction view: `baseSize * 1.30f` at 48pt texture size. All `DrawTextEx`/`MeasureTextEx` calls in prospecting panel methods are wrapped with `FS()`. This keeps text size consistent across all extraction unit panels.

## Open Questions

| Question | Status |
|----------|--------|
| Should all three stage tabs be visible simultaneously, or only the active stage? | [?] |
| How does the sample overview bar interact with stage content? (click sample → jump to its stage?) | [?] |
| How much screen real estate does the NxN grid need vs the sample tray? | [?] |
| 20 ore shape template designs | **Resolved:** 4 families × 5 templates, with depth-family affinity bias |
| Where does the stratigraphic column view live? | **Resolved:** Side panel on hover/select |
| Does the UI need to work at the current extraction view's font scale? | **Resolved:** Yes — same FS() (1.30x at 48pt) |
| Where do objectives live within the prospecting panel? | **Resolved:** Bottom collapsible section |
