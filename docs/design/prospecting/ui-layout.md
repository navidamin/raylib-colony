# Prospecting UI Layout

> Status: STUB
> Last Updated: 2026-04-01
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
│  │  [Rotating sample icons showing all samples across   │   │
│  │   all stages with visual state indicators]           │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─ Message Bar ───────────────────────────────────────┐   │
│  │  Pathfinder tips / Status messages / Alerts          │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Stage-Specific Views

### Sweep View (Phase 0)
[?] — Layout for:
- 5x5 grid with confidence heat map overlay
- Frequency/depth slider
- Sweep button + energy cost display
- Results summary

### Samples View (Phase 1+2)
[?] — Layout for:
- Grid for cell selection (sampling target)
- Depth layer selector
- Sample tray inventory
- Initial data display per sample
- Pipeline design interface (tool assignment per sample)

### Lab View (Phase 3)
[?] — Layout for:
- Processing queue / batch slots
- Active processing animation
- Incoming results data stream
- Completed results archive
- Stratigraphic column display (7E)

## Sample Visual Design

Samples should be visually distinguishable. Rotating icons in the overview bar.

[?] — Visual encoding:
| Property | Encoding Option | Alternative |
|----------|----------------|-------------|
| Depth layer | Shape? (cube=surface, cylinder=core, crystal=deep) | Color band? |
| Stage | Glow/animation? (static=tray, spinning=processing, bright=done) | Border color? |
| Quality/confidence | Brightness? Size? | Star rating overlay? |
| Element content | Tint color? (red=iron, blue=water, etc.) | Icon overlay? |

[?] — "Varied shapes" concept needs concrete visual design

## Interaction Flow

```
Player opens Prospecting menu
    │
    ├─► Sweep tab: Configure and run GPR sweeps
    │       └─► Results update grid heat map
    │
    ├─► Samples tab: Select cells, choose depths, collect
    │       ├─► New sample appears in tray
    │       └─► Assign processing pipeline (or accept default)
    │
    └─► Lab tab: Monitor processing, view results
            ├─► Completed samples show element data
            ├─► Pathfinder tips appear in message bar
            └─► Survey progress updates per cell
```

## Open Questions

- Should all three stage tabs be visible simultaneously, or only the active stage?
- How does the sample overview bar interact with stage content? (click sample → jump to its stage?)
- Where does the stratigraphic column view live? Separate tab? Overlay on grid?
- How much screen real estate does the 5x5 grid need vs the sample tray?
- Does the UI need to work at the current extraction view's font scale (FS() multiplier)?
