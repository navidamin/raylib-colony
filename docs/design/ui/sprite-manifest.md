# Sprite Manifest — Module Panels for Non-Extraction Units

Status: PLANNED (assets not yet produced)
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

| Sprite set | Variants | Count | Used by |
|---|---|---|---|
| **Part blanks** | 4 materials (Fe/Si/Ti/Al) x 3 process stages (raw / machined / finished) | 12 | Fabrication, Assembly, Quality |
| Conveyor tile | 4 animation frames | 4 | Assembly |
| Robot arm | 3 poses (reach / grip / place) | 3 | Assembly, Automation |
| QC stamp overlays | pass / fail / pending | 3 | Quality |
| Cargo crates | 3 sizes x 2 states (open/sealed) | 6 | Logistics |
| Status hologram | 3 states (nominal / warning / fault) | 3 | Automation |

Part blanks tinted by material color reuse the element-tint path already in
`DrawCrystalSprite`.

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
| Manufacture | 31 |
| Research | 19 (+1 overlay) |
| Shared | 4 (optional) |
| **Total** | **~185** |

Well under the crystal set's 400 — one `crystal_gen`-style generator pass per
family of sets. Recommended production order: **battery cells** (smallest,
validates the pipeline on a second unit), then **crop growth stages**
(biggest gameplay payoff), then part blanks, then the research reuse pass.
