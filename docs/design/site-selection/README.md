# Site Selection — Design Documents

**Status: IN PROGRESS** — design settled; step 1 of 6 implemented.

How a player gets from "somewhere on the Moon" to "the base goes *here*":
a continuous zoom in which a survey cursor aggregates what is under it,
scale by scale, until the final 1 km step where the site is committed.

## Documents

| Document | Purpose | Status |
|----------|---------|--------|
| [site-selection-master-design.md](site-selection-master-design.md) | The descent ladder, cursor behaviour, aggregation model, commit step | SETTLED |

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
| `src/TerrainGen/survey_cursor.{h,cpp}` | **The ladder and cursor geometry** — screen ↔ km ↔ lat/lon, grid snapping, descent stack (step 1) |
| `tools/surveycursor/survey_cursor_test.cpp` | Headless self-test for the above |
| `tools/lunarmap/lunarmap_main.cpp` | `--ladder` walks the descent; `--place` / `--survey` prototype the readout |

**Related design docs**

- [`docs/design/prospecting/README.md`](../prospecting/README.md) — what happens *after* a site is chosen; the survey cursor is deliberately coarser than prospecting instruments
- [`docs/design/sect-view/README.md`](../sect-view/README.md) — the view the player lands in once built

**Roadmap**

- `ROADMAP_IMMINENT.md` — site selection is part of the colony founding loop

## Progress

| Step | State |
|------|-------|
| 1 — Cursor infrastructure | **done** — `survey_cursor.{h,cpp}`, self-test, `lunar_map --ladder` |
| 2 — Aggregation | not started |
| 3 — Readout panel | not started |
| 4 — Ladder wiring | not started |
| 5 — Commit step | not started |
| 6 — Presentation sharpening | not started |

## Open questions

Tracked with `[?]` in the master design. None outstanding.

**Decided:** the cursor grid-snaps at navigation levels and moves freely
at level 5; descent is fully reversible and only the final build commits.

**Proposed, awaiting confirmation:** confidence is **per quantity, not
per zoom level** (§4.3). Descending is a camera move, not an instrument
change, so it resolves terrain completely and never resolves chemistry:
resource readouts are averaged over their *instrument's* footprint, which
stays far larger than the base. Below that floor the value freezes and
the **uncertainty band widens** — the measurement did not degrade, the
question got sharper. Terrain bands close as resource bands open, which
states what the descent buys without a word of text. Nothing is biased or
noised. This supersedes the earlier "resolution-limited" decision, which
made uncertainty a function of zoom and so made it a toll rather than a
decision. §4.4 lists what the floors force us to decide next — chiefly
that the resource generator must put structure *below* the instrument
floor or the whole mechanic is inert.
