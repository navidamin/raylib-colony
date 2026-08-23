# Sect View Module — Design Documents

> **Auto-context rule:** When working on sect view rendering (`src/Sect/sect.cpp`
> `DrawInSectView` and its helpers, `src/Engine/rendermanager.cpp` `DrawSectView`,
> sect view input handling), read this README first to load design context.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [sect-view-elements.md](sect-view-elements.md) | HUD element inventory, surroundings brainstorm, hover tooltip design | DRAFT |

## Design Summary

The sect view is the base-management screen: a top-down "orbital layout" of one
sect — a central hex-glass dome (development readout) surrounded by 8 unit dome
stations, linked by connector arms with status LEDs, enclosed by a ring road
with entry rails leading off-screen.

**Visual design status: IMPLEMENTED** (branch `claude/section-visual-redesign-k001sc`):

- Per-pixel ray-shaded dome spheres (Lambert + two-lobe Blinn specular +
  fresnel + bounce light), baked into cached textures per tint/size/seed
- Per-unit lighting character seeded from the unit type name (FNV-1a)
- Hex glass pattern, riveted bezels, procedural unit glyphs (no sprite assets)
- Connector arms with green conduits + socket LEDs showing unit status
- Ring road with crossbar lamp seams; twin entry rails with gate boxes

Open design work (this module's documents): what surrounds the base, what the
HUD shows, and what hovering each element reveals — without cluttering the
scene. See [sect-view-elements.md](sect-view-elements.md).

## Cross-References

### Source Code (current implementation)
| File | Relevant Code |
|------|--------------|
| `src/Sect/sect.cpp` | `DrawInSectView` + anonymous-namespace visual helpers (dome baking, glyphs, arms, ring road, rails) |
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
