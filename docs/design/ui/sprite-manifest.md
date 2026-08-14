# Sprite Manifest — Module Panels for Non-Extraction Units

Status: PLANNED (assets not yet produced)

> **Scope note.** This manifest covers Farming, Energy, Manufacture, and
> Research. The game instantiates **eight** unit types — see
> `Sect::CreateInitialUnits` in `src/Sect/sect.cpp`, which is the authoritative
> list. **Construction, Transport, and Communication** now have named modules
> and procedural icons but no sprite sets planned here yet. Their hero visuals
> are the obvious candidates when they are designed: a structure rising through
> build stages, a hauler with load states, and a dish/signal-strength set.
Owner: UI / art pipeline
Related: `tools/crystal_gen/` (the prospecting crystal sprite generator — the
same pre-render-3D-to-PNG pipeline should produce most of the sets below),
`docs/design/prospecting/ui-layout.md` (theme reference).

## Context

The extraction unit UI is fully themed (dark sci-fi kit, procedural line
icons, floating cards). Its one bespoke asset set is the crystal samples:
**4 families x 5 shapes x 4 sizes x 5 glow levels = 400 sprites**, generated
by `tools/crystal_gen`, keyed by a small struct (`CrystalVisual`) and drawn
via a lazy texture cache in `RenderManager`.

When Farming / Energy / Manufacture / Research get their real module panels
(they are stub modules today), each needs the same two ingredient types:

1. **A state-varying "hero" sprite set** — the thing the player inspects and
   watches change (the analog of the crystals).
2. **Small support sprites** — slot fillers, gauges, and markers that the
   procedural kit cannot express well.

Module *icons* for the left-hand module list do NOT need sprites — the
procedural `ExtIcon` set should be extended instead (one glyph per module,
~10 lines of raylib primitives each).

## Farming (Irrigation, Greenhouse, Hydroponics, Harvest, Storage)

| Sprite set | Variants | Count | Used by |
|---|---|---|---|
| **Crop growth stages** | 4 crop types x 5 growth stages x 3 health states | 60 | Greenhouse, Hydroponics, Harvest |
| Moisture overlay tiles | 5 saturation levels (grid heat-map analog) | 5 | Irrigation |
| Pipe segments | straight, corner, T-junction, sprinkler head | 4 | Irrigation |
| Nutrient tank | 4 fill levels | 4 | Hydroponics |
| Produce crates | 4 crop types x 3 fill levels | 12 | Harvest, Storage |
| Spoilage indicator | 3 stages (fresh / aging / spoiled) | 3 | Storage |

Crop growth is the farming "crystal set": same generator approach, seeded
per-plot so a greenhouse grid reads at a glance.

## Energy (Solar Array, Battery, Nuclear, Grid, Emergency)

| Sprite set | Variants | Count | Used by |
|---|---|---|---|
| **Battery cells** | 5 charge levels x 3 wear states | 15 | Battery |
| Solar panel tiles | clean / dusty / damaged x 4 sun-angle tints | 12 | Solar Array |
| Reactor core glow | 5 output levels (idle to overdrive) | 5 | Nuclear |
| Fuel rods | fresh / half-spent / spent | 3 | Nuclear |
| Grid pylon + cable segment | pylon, cable straight, cable sag | 3 | Grid (also Colony view roads later) |
| Alarm beacon | off / armed / alert (2-frame flash) | 4 | Emergency |

Battery charge/wear is the energy "crystal set" — tray of cells, glow =
charge, exactly the crystal drawing path.

## Manufacture (Fabrication, Assembly, Quality, Logistics, Automation)

> Detailed manifest — this unit's panels are envisioned as a **production
> line view** (stations + conveyor + parts in flight), a **QC bench** (the
> lab-tab analog: pick a part, inspect it), a **logistics bay**, and an
> **automation console**. Every set below is keyed by a small struct, drawn
> through the same lazy texture cache as the crystals.

### Intended panel layout (context for the sprites)

```
FABRICATION / ASSEMBLY tab            QUALITY tab (lab analog)
+--------------------------------+    +------------------------------+
| [station] ==conveyor== [station]|   | SELECT PART   RESULTS        |
|     |         part->      |    |    | [slot][slot]  [big part      |
| [station] ==conveyor== [output]|    | [slot][slot]   preview +     |
|  station cards w/ state glow   |    |  stamp overlay on each]      |
+--------------------------------+    +------------------------------+
```

### Hero set: part blanks (`sprites/manufacture/parts/`)

The workpiece is the "crystal" of this unit — one object inspected and
watched as it transforms. Sprites are rendered **neutral gray**; material
is applied as a runtime tint (same lift-toward-white path as
`DrawCrystalSprite`), so one file serves every material.

| Axis | Values | Notes |
|---|---|---|
| Process stage | `raw` / `machined` / `finished` | geometry changes per stage |
| Shape template | 3 per stage | raw: ingot, billet, casting; machined: bracket, cylinder, plate; finished: gear, housing, casing |
| Material | Fe / Si / Ti / Al (+ future alloys) | runtime tint, **zero extra files** |
| Defect | overlay sprite per stage (crack, void, warp) | composited on top, any part can show a flaw |

Files: `parts/<stage>/<template>.png` (9) + `parts/defects/<stage>.png` (3) = **12**.

Keying struct (mirrors `CrystalVisual`):

```cpp
struct PartVisual
{
    PartStage stage;        // RAW, MACHINED, FINISHED
    int templateIndex;      // 0-2 within stage
    ResourceType material;  // tint source (Fe, Si, Ti, Al)
    bool defective;         // draw defect overlay
};
```

### Production line (`sprites/manufacture/line/`)

| Sprite set | Variants | Files | Used by |
|---|---|---|---|
| Machine stations | 3 stations (furnace, CNC, press) x 3 states (idle / active / fault) | 9 | Fabrication center panel — station cards like the separation chain |
| Conveyor belt | straight + corner, 4 animation frames each | 8 | Assembly line view; direction via rotation at draw time |
| Robot arm | 4 poses (idle / reach / grip / place) | 4 | Assembly, Automation; tier via tint |

Station `active` state should read at a glance (glow window, moving tool),
`fault` gets a red edge — matches the ON/OFF border language used in the
beneficiation chain.

### Quality bench (`sprites/manufacture/qc/`)

| Sprite set | Variants | Files | Used by |
|---|---|---|---|
| Stamp overlays | pass / fail / pending | 3 | composited over part slots, like the tray state borders |
| Scanner beam | 2-frame sweep | 2 | inspection animation over the big part preview |
| Defect close-ups | crack / void / warp | 3 | RESULTS detail view when a part fails |

### Logistics bay (`sprites/manufacture/logistics/`)

| Sprite set | Variants | Files | Used by |
|---|---|---|---|
| Crates | 3 sizes x 2 states (open / sealed) | 6 | outgoing goods display, fill = production backlog |
| Pallet | empty / loaded | 2 | staging area |
| AGV cart | idle / moving (2 frames) | 2 | animated between stations and bay |

### Automation console (`sprites/manufacture/automation/`)

| Sprite set | Variants | Files | Used by |
|---|---|---|---|
| Status holograms | nominal / warning / fault x 2-frame pulse | 6 | console header, mirrors kit orb language |
| Console bench | 3 tier levels | 3 | center art, tier-gated detail |
| Program chips | 3 routine types (throughput / quality / balanced) | 3 | clickable policy selector cards |

### Manufacture totals and production notes

| Set | Files |
|---|---|
| Part blanks + defects | 12 |
| Production line | 21 |
| Quality bench | 8 |
| Logistics bay | 10 |
| Automation console | 12 |
| **Manufacture total** | **63** |

- Render at the crystal set's resolution via a `part_gen` sibling of
  `tools/crystal_gen` (same lighting shader; parts are simple lathe/extrude
  geometry, cheaper than crystals).
- Animation is frame-swap by game time — no atlas needed; the texture cache
  already handles per-file loading.
- Priority within the unit: **part blanks first** (they appear in three of
  five module panels), then stations + conveyor (the line view is the
  unit's identity), then QC, logistics, automation.
- Revises the earlier rough count (31) — the detailed design roughly
  doubles it, still one generator pass.

## Research (Laboratory, Analysis, Simulation, Archive, Publication)

| Sprite set | Variants | Count | Used by |
|---|---|---|---|
| **Hologram globes** | 3 colors x 3 detail levels (the kit's orb sprites) | 9 | Simulation, Analysis |
| Instrument bench | 3 tier levels | 3 | Laboratory |
| Specimen jars | reuse crystal sprites inside a jar frame overlay | 1 overlay | Laboratory |
| Data cartridges / server rack | rack + 4 fill levels | 5 | Archive |
| Transmission glyph | 2-frame pulse | 2 | Publication |

Research deliberately reuses the crystal set (specimens) and the kit's orb
art (simulations) — the smallest new-asset budget of the four units.

## Shared / chrome

| Sprite set | Variants | Count | Used by |
|---|---|---|---|
| Unit blueprint wireframes | 1 per unit type (FU / EN / MF / RS, like UE-3) | 4 | Control panel art (or keep procedural) |
| Unit icon chip glyphs | 1 per unit type | 0 | Extend procedural `ExtIcon` instead |
| Module list icons (20 stub modules) | 1 per module | 0 | Extend procedural `ExtIcon` instead |

## Totals

| Unit | New sprites |
|---|---|
| Farming | 88 |
| Energy | 42 |
| Manufacture | 63 |
| Research | 19 (+1 overlay) |
| Shared | 4 (optional) |
| **Total** | **~215** |

Well under the crystal set's 400 — one `crystal_gen`-style generator pass per
family of sets. Recommended production order: **battery cells** (smallest,
validates the pipeline on a second unit), then **crop growth stages**
(biggest gameplay payoff), then part blanks, then the research reuse pass.
