# Sect View Elements — HUD, Surroundings, Hover

Status: DRAFT

Brainstorm inventory for everything the sect view could show beyond the base
structure itself. Guiding principle throughout: **the scene stays diegetic**
(no floating text/numbers on the structure or terrain); anything readable lives
in the HUD or in on-demand tooltips. One number should appear in exactly one
place.

Legend for data sources: `✓` exists in code today, `◐` partially exists /
needs a small system, `[?]` open design question.

---

## 1. HUD Element Inventory

Three layers, from always-visible to on-demand.

### Layer 1 — Always visible, diegetic (drawn on the structure)

| Element | What it shows | Data source |
|---|---|---|
| Unit status rings + socket LEDs | Active/idle per unit | ✓ implemented |
| Spoke conduit pulse | Animated flow along the arm when a unit is actually producing | ✓ production events |
| Development arc | Fill arc around the dome collar (alternative/addition to the text readout) | ✓ `development_percentage` |
| Construction scaffolding | Yellow-striped partial ring + radial progress arc on a station being upgraded | ✓ construction timers |
| Storage silo gauge | Silo cluster on/near the ring whose fill level shows total storage usage, amber/red near capacity | ✓ storage maps |
| Day/night lighting | Directional dome light, station lamps at night; explains solar energy income | ✓ `TimeManager.GetTimeOfDay()` |
| Roads out + vehicles | Entry rails toward neighbor sects; tiny vehicle when a transfer fires | ◐ `PushSurplusToColony` events |

### Layer 2 — Glanceable compact HUD (fixed panels)

| Element | What it shows | Data source |
|---|---|---|
| Critical resources row | Energy, Food, Water, Manpower only — icon + bar; full breakdown moves to hover | ✓ storage maps |
| Net rates | Small +/- per day next to the four key resources (trend, not just level) | ◐ needs rate tracking |
| Alert badges | Warning chip on a blocked station (no energy / storage full / missing input) | ◐ needs blocker detection |
| Alert feed | "Updates" panel gets a purpose: newest alerts on top | ◐ needs event capture |
| Sect name + archetype tag | One line, top center: "Sect 3 — Mare Industrial" | ✓ colony archetype |
| Manpower gauge | Single bar; the sect-wide shared constraint | ✓ MANPOWER storage |
| Colony link chip | Last surplus push / deficit pull: "→ Colony: Fe 40 (Day 12)" | ◐ needs transfer log |

### Layer 3 — On demand only (hover/click, invisible otherwise)

| Element | Trigger | Content |
|---|---|---|
| Unit tooltip | Hover unit dome | See section 3 |
| Sect card | Hover central dome | Development %, storage summary (top 4 + "N more…"), manpower, storage level |
| Colony exchange summary | Hover rails/gates | Last push/pull per resource |
| Storage upgrade panel | Click storage gauge | Current bottom-right panel, hidden until affordable/clicked |
| Construction popup | Hover scaffolded station | Job, time remaining, materials reserved |

### Deliberately excluded from this view

Research/tech tree state (Research unit view), per-module details (Unit view),
planet/orbital data (Site Selection view), duplicate readouts of any number.

---

## 2. Surroundings (outside the ring road)

### Terrain & context (passive, sets the scene)

| Element | What it shows | Data source |
|---|---|---|
| Terrain relief | Real DEM-based ground | ✓ exists |
| Resource outcrops | Faint ore-tinted patches / crystal clusters hinting what's rich in this cell | ✓ `ResourceManager` cell abundances |
| Crater rims / boulders | Dressing; breaks up empty corners | ✓ terrain synthesis |
| Site archetype cue | Subtle palette shift (KREEP glow, polar frost, mare basalt) | ✓ colony archetype |
| Earth in the sky / horizon glow | Corner vignette tied to `earthVisibility` | ✓ orbital survey data |
| Day/night lighting | Terrain darkens; road lamps + dome glows carry the night | ✓ `TimeManager` |

### Live/functional elements (inform, not just decorate)

| Element | What it shows | Data source |
|---|---|---|
| Roads to neighbor sects | Direction + existence of connections; entry rails become real links | ◐ `roadsUnderConstruction` |
| Cargo rover on rails | Animates when surplus push / deficit pull fires | ◐ colony transfer events |
| Excavation site | Dig pit + excavator dots outside the ring while Extraction runs; count = fleet size | ✓ Excavation module |
| Prospecting scan sweep | Brief scan-line arc across terrain on a LIBS scan | ✓ Prospecting module |
| Construction site | Scaffold + crane props near the ring during unit upgrade | ✓ construction timers |
| Solar field | Panel rows that tilt/catch light with time of day (explains ambient energy) | ✓ ambient energy system |
| Depletion scarring | Terrain grays where resources are drawn down | ✓ depletion tracking |

**Rule:** no text, numbers, or bars in the surroundings — purely diegetic.

---

## 3. Hover Tooltips Per Unit

### Common card frame (every unit, ~5 lines max)

| Line | Content |
|---|---|
| Header | Glyph + name + status chip (Active / Idle / Blocked / Upgrading) |
| Modules | 5 module pips with tier numbers (0–3) |
| Rates | Producing → X/day, Consuming → Y/day |
| Blocker | One red line, only when blocked: "No energy" / "Storage full" / "Missing input" |
| Footer | "Click to open" hint |

### Type-specific middle section

| Unit | Tooltip specifics | Data source |
|---|---|---|
| **Extraction** | Survey progress %, scan multiplier, site marked?, excavators (n, avg wear), active directive, top output resource | ✓ all exists (richest unit) |
| **Farming** | Food output/day, water draw/day, manpower assigned | ✓ production costs |
| **Energy** | Generation/day vs ambient baseline, storage fill %, day/night factor now | ✓ energy system |
| **Manufacture** | Current recipe (typed resource), inputs consumed, queue count | ◐ typed production partial |
| **Transport** | Road connections (n), last transfer (what/how much/when), capacity | ◐ needs transfer log |
| **Communication** | Colony link status, deficit/surplus signals broadcast today | ◐ needs event capture |
| **Research** | Science/day, current tech target + %, unlocked count (of 14) | ◐ registry exists, progress doesn't |
| **Construction** | Active build/upgrade job, time remaining, materials reserved | ◐ timers exist, job list doesn't |

### Hover behavior rules (anti-clutter)

- One tooltip at a time, ~250 ms delay, anchored beside the dome, never
  covering the center
- Numbers only in tooltips — the dome keeps at most its status ring + one
  alert badge
- Same dark panel style as the rest of the sect view UI

---

## 4. Priority Order

1. **Common tooltip card frame** — works for all 8 units immediately
2. **Extraction tooltip specifics** — all data already exists
3. **Blocker line + alert badges** — highest gameplay value per pixel
   ("why is nothing happening")
4. Sect name/archetype line + net rates on the key resources
5. Surroundings cheap wins: resource outcrops, excavation site, solar field
6. Day/night lighting pass
7. Transfer log → colony link chip, rover animation, Transport/Communication
   tooltip data

## 5. Gaps Inventory

- `[?]` Blocker detection: units don't currently report *why* they can't
  produce — needs a `GetBlockReason()` on Unit
- `[?]` Rate tracking: per-day production/consumption deltas aren't recorded
  anywhere
- `[?]` Transfer log: colony push/pull happens silently; needs a small ring
  buffer of recent transfers (what, amount, day)
- `[?]` Manpower assignment: units don't reserve manpower yet, so "assigned"
  numbers have nothing to read
- `[?]` Display names: unit type strings ("Extraction") vs friendlier labels
  ("Mining") — decide once tooltips ship
- `[?]` Tooltip trigger on gamepad/touch: hover-only design assumes mouse
