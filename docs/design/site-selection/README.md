# Site Selection — Design Documents

**Status: DRAFT** — design written, not yet implemented.

How a player gets from "somewhere on the Moon" to "the base goes *here*":
a continuous zoom in which a survey cursor aggregates what is under it,
scale by scale, until the final 1 km step where the site is committed.

## Documents

| Document | Purpose | Status |
|----------|---------|--------|
| [site-selection-master-design.md](site-selection-master-design.md) | The descent ladder, cursor behaviour, aggregation model, commit step | DRAFT |

## The idea in one paragraph

At every zoom level the player moves a translucent rectangle over the
terrain. The rectangle is always the *next* level's footprint, so it
answers one question — "if I descend here, what am I descending into?"
Inside it the player sees the **aggregated** resource and terrain
readout for that area: coarse and uncertain when zoomed out, sharp and
specific when zoomed in. The descent ends at the 1 km scale, where the
cursor stops being a navigation tool and becomes a **build footprint**:
the visuals change register, buildability gating engages, and the site
is committed.

## Cross-references

**Source files**

| File | Relevance |
|------|-----------|
| `src/TerrainGen/lola_dem.{h,cpp}` | `EvaluateSite` / `TerrainBuildability` — the real terrain gate |
| `src/TerrainGen/terrain_synthesis.h` | `TerrainGridCellToLatLon`, `OrbitalPickToLatLon`, `TERRAIN_CELL_KM` |
| `src/ResourceManager/resource_manager.{h,cpp}` | `OrbitalSurveyData`, `GetSiteArchetype`, survey grid |
| `src/Engine/gamemanager.cpp` | Existing `View::SITE_SELECTION` flow, Ctrl+click placement |
| `src/Engine/rendermanager.cpp` | `DrawSiteSelectionView`, instrument panels |
| `src/Engine/viewmanager.cpp` | View transitions and camera setup |
| `tools/lunarmap/lunarmap_main.cpp` | `--place` / `--survey` prototype of the cursor and readout |

**Related design docs**

- [`docs/design/prospecting/README.md`](../prospecting/README.md) — what happens *after* a site is chosen; the survey cursor is deliberately coarser than prospecting instruments
- [`docs/design/sect-view/README.md`](../sect-view/README.md) — the view the player lands in once built

**Roadmap**

- `ROADMAP_IMMINENT.md` — site selection is part of the colony founding loop

## Open questions

Tracked with `[?]` in the master design.

**Decided:** cursor grid-snaps at navigation levels and moves freely at
level 5; descent is fully reversible and only the final build commits.

**Still open:** `[?]` whether high-altitude readouts are merely
resolution-limited (recommended), imprecise, or actively biased — see
§4.3.
