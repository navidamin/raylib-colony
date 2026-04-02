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
│  ┌─ Objectives Panel ─────────────────────────────────┐   │
│  │  [Active objectives with progress indicators]       │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ Message Bar ───────────────────────────────────────┐   │
│  │  Pathfinder tips / Status messages / Alerts          │   │
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

### Shape Templates

20 procedural ore shape templates to be designed:
- 4-7 triangles/rectangles per template
- Varied silhouettes (angular chunks, crystalline shards, rounded nodules, layered slabs)
- Each sample randomly selects a template, colored by dominant element
- Templates are visual variety only — no gameplay meaning

[?] Exact 20 shape template designs — to be created with visual prototyping

## Objectives Display

### Objectives Panel

Located below the Sample Overview Bar, shows active tier-locked tutorial objectives:

```
┌─ Objectives ──────────────────────────────────────────┐
│  ☐ Complete your first surface sweep          [T0]    │
│  ☐ Collect 3 samples from different cells     [T0]    │
│  ☑ Run XRF analysis on a sample               [T1]    │
│  ☐ Achieve "High" confidence on any cell      [T1]    │
└───────────────────────────────────────────────────────┘
```

- Objectives are **tier-locked** — new objectives appear when the tier unlocks
- **No deadlines** — objectives persist indefinitely until completed
- **Rewards** are mixed capability unlocks per objective type:
  - Bonus tray slots
  - AI behavior upgrades
  - Early tool/preset unlocks

[?] Exact objective list per tier — to be designed during implementation

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

## Open Questions

| Question | Status |
|----------|--------|
| Should all three stage tabs be visible simultaneously, or only the active stage? | [?] |
| How does the sample overview bar interact with stage content? (click sample → jump to its stage?) | [?] |
| Where does the stratigraphic column view live? Separate tab? Overlay on grid? | [?] |
| How much screen real estate does the 5x5 grid need vs the sample tray? | [?] |
| Does the UI need to work at the current extraction view's font scale (FS() multiplier)? | [?] |
| 20 ore shape template designs | [?] — to be prototyped |
