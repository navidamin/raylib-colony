# Sect View Module — Design Documents

> **Auto-context rule:** When working on sect view rendering (`src/Sect/sect.cpp`
> `DrawInSectView` and its helpers, `src/Engine/rendermanager.cpp` `DrawSectView`,
> sect view input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [sect-view-elements.md](sect-view-elements.md) | HUD element inventory, surroundings brainstorm, hover tooltip design | DRAFT |
| 2 | [domeforge-study.md](domeforge-study.md) | **The current visual design:** the DomeForge base (`prototypes/dome-forge/`, ported to `src/DomeForge/`), the user's decisions, how it is baked and placed, the levelled ground under it, and the 3D view's sizing | IMPLEMENTED (2D) |

## Design Summary

The sect view is the base-management screen: a top-down "orbital layout" of one
sect — a central hex-glass dome (development readout) surrounded by 8 unit dome
stations, linked by connector arms with status LEDs, enclosed by a ring road
with entry rails leading off-screen.

**Visual design status: DomeForge base, IMPLEMENTED** (branch
`claude/sect-view-domeforge`). See [domeforge-study.md](domeforge-study.md):

- The user's DomeForge art set (`prototypes/dome-forge/`), ported bit-exact to
  `src/DomeForge/` and baked once per game by `src/Sect/sect_art.cpp`
- Green glass = unit on, grey = off; unit glyph + label on the glass
- Kerbed roads filleted into the dome rims, socket lights; no entry rails
- Drawn at its real size on the ground (ring road 1.10 km from the centre),
  over terrain levelled under the base's footprint
- The previous art (ray-shaded dome stations) is recorded in
  `docs/graveyard.md` §11

Open design work (this module's documents): what surrounds the base, what the
HUD shows, and what hovering each element reveals — without cluttering the
scene. See [sect-view-elements.md](sect-view-elements.md).

## Cross-References

### Source Code (current implementation)
| File | Relevant Code |
|------|--------------|
| `src/Sect/sect.cpp` | `DrawInSectView`: places the art, draws glyphs/labels and the development readout, sets unit hit areas |
| `src/Sect/sect_art.cpp` | The DomeForge set: time-sliced bake, layout on screen, draw calls |
| `src/DomeForge/` | The DomeForge port (sprites, roads, layout; 3D scaffold) |
| `tools/domeforge/` | JS-vs-port diff gate |
| `src/Sect/sect.h` | Sect entity, storage, units |
| `src/Engine/rendermanager.cpp` | `DrawSectView` (terrain background, resource dashboard, storage upgrade panel) |
| `src/Engine/inputmanager.cpp` | Unit click detection via `SetUnitPosInSectView` / `SetUnitRadiusInSectView` |
| `src/TerrainGen/terrain_synthesis.cpp` | Sect terrain background generation |

### Existing Documentation
| Document | Location | Relevance |
|----------|----------|-----------|
| Overall Roadmap | `ROADMAP_OVERALL.md` (root) | Phase tracking |
| Imminent Roadmap | `ROADMAP_IMMINENT.md` (root) | Current sprint tasks |

### Related Module Designs
| Module | Dependency |
|--------|------------|
| `prospecting/` | Extraction unit hover data (survey progress, scan multiplier, excavators, directives) |
| `research/` | Research unit hover data (tech target, progress) once UnlockRegistry is real |
