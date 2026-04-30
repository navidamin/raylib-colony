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

## Sample Visual Design (Resolved)

### Procedural Ore Shapes

Each sample is rendered as a **procedural ore shape** — not a generic icon:

```
┌─ Sample Icon Structure ──────────────────────────┐
│                                                    │
│   ┌──────── Circular Border ────────┐             │
│   │  ○ Ring count = depth layer     │             │
│   │    1 ring  = Regolith (surface) │             │
│   │    2 rings = Megaregolith       │             │
│   │    3 rings = Fractured Bedrock  │             │
│   │    4 rings = Intact Bedrock     │             │
│   │                                 │             │
│   │   ┌── Ore Shape ──┐            │             │
│   │   │ 4-7 polygon    │            │             │
│   │   │ fragments       │            │             │
│   │   │ element-colored │            │             │
│   │   │ glow=confidence │            │             │
│   │   └────────────────┘            │             │
│   └─────────────────────────────────┘             │
│                                                    │
└────────────────────────────────────────────────────┘
```

### Visual Encoding

| Property | Encoding |
|----------|----------|
| **Element content** | Color of ore fragments (Fe=red-brown, H₂O=blue, Ti=silver, He-3=purple, etc.) |
| **Confidence level** | Glow intensity around ore shape (no glow=Very Low, bright glow=Certain) |
| **Depth layer** | Ring count on circular border (1-4 rings) |
| **Processing state** | Static=tray, spinning=processing, bright flash=complete |

### Shape Templates (Resolved)

20 procedural ore shape templates in 4 visual families. Each template is a list of 4-7 convex polygon fragments arranged within a 32x32 unit cell. Samples randomly select a template at creation, colored by dominant element. Templates are visual variety only — no gameplay meaning.

#### Family A: Angular Chunks (fractured rock, sharp edges)

| # | Name | Fragments | Silhouette Description |
|---|------|-----------|----------------------|
| A1 | Cleaved Block | 4 | Two large rectangular halves with offset fracture line |
| A2 | Shatter | 7 | Radial fragments from center impact point |
| A3 | Wedge Pair | 4 | Two opposing triangular wedges with gap |
| A4 | Stacked Fracture | 5 | Horizontal layers with jagged breaks between them |
| A5 | Corner Break | 5 | Large block with one corner chipped into 2 small pieces |

#### Family B: Crystalline Shards (elongated, pointed, mineral-like)

| # | Name | Fragments | Silhouette Description |
|---|------|-----------|----------------------|
| B1 | Single Crystal | 4 | One tall hexagonal prism with 3 small chip fragments |
| B2 | Twin Growth | 5 | Two intersecting elongated hexagons + 3 tiny chips |
| B3 | Needle Cluster | 6 | 6 thin elongated triangles radiating from center |
| B4 | Tabular | 4 | One wide flat hexagon + 3 small prismatic fragments |
| B5 | Druzy | 7 | Flat base with 5-6 small pointed fragments on top surface |

#### Family C: Rounded Nodules (smooth, weathered, organic shapes)

| # | Name | Fragments | Silhouette Description |
|---|------|-----------|----------------------|
| C1 | Cobble | 4 | One large rounded rectangle + 3 small oval chips |
| C2 | Botryoidal | 6 | Cluster of 6 overlapping circles/ovals (grape-like) |
| C3 | Concretion | 4 | Large oval with 3 crescent-shaped shell fragments |
| C4 | Pebble Scatter | 7 | 7 small rounded polygons in loose cluster |
| C5 | Split Nodule | 5 | Two halves of an oval + 3 interior fragments exposed |

#### Family D: Layered Slabs (sedimentary, flat, stacked)

| # | Name | Fragments | Silhouette Description |
|---|------|-----------|----------------------|
| D1 | Flagstone | 4 | 4 thin wide rectangles stacked with slight offsets |
| D2 | Shale Split | 6 | 6 very thin irregular parallelograms, fanning apart |
| D3 | Cross-Bedded | 5 | 3 diagonal slabs crossed by 2 thin perpendicular pieces |
| D4 | Laminate | 5 | 5 horizontal strips of varying width, tight stack |
| D5 | Breccia Slab | 7 | Flat slab outline filled with 6 angular fragment inclusions |

#### Depth-Family Affinity (soft rule, not strict)

Templates are randomly assigned, but with a bias toward geologically plausible families per depth layer:

| Depth Layer | Preferred Families | Rationale |
|-------------|-------------------|-----------|
| Regolith | C (Nodules), D (Slabs) | Weathered surface material |
| Megaregolith | A (Chunks), D (Slabs) | Impact-fractured rubble |
| Fractured Bedrock | A (Chunks), B (Crystals) | Fractured mineral veins |
| Intact Bedrock | B (Crystals), D (Slabs) | Pristine crystalline rock |

Selection: 70% chance from preferred families, 30% from any family.

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
